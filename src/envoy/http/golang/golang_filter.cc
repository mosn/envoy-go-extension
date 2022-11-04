#include "src/envoy/http/golang/golang_filter.h"

#include <cstdint>
#include <string>
#include <vector>

#include "envoy/http/codes.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/base64.h"
#include "source/common/common/enum_to_int.h"
#include "source/common/common/utility.h"
#include "source/common/grpc/common.h"
#include "source/common/grpc/context_impl.h"
#include "source/common/grpc/status.h"
#include "source/common/http/headers.h"
#include "source/common/http/http1/codec_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

std::atomic<uint64_t> Filter::global_stream_id_;

void Filter::onHeadersModified() {
  // Any changes to request headers can affect how the request is going to be
  // routed. If we are changing the headers we also need to clear the route
  // cache.
  decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
}

Http::LocalErrorStatus Filter::onLocalReply(const LocalReplyData& data) {
  ASSERT(isThreadSafe());
  ENVOY_LOG(debug, "golang filter onLocalReply, state: {}, phase: {}, code: {}", int(state_),
            int(phase_), int(data.code_));

  // TODO: let the running go filter return a bit earilier, by setting a flag?
  return Http::LocalErrorStatus::Continue;
}

void Filter::phaseGrow(int n) {
  ENVOY_LOG(debug, "phase grow, from {} to {}", static_cast<int>(phase_),
            static_cast<int>(phase_) + n);
  phase_ = static_cast<Phase>(static_cast<int>(phase_) + n);
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(debug, "golang filter decodeHeaders, state: {}, phase: {}, end_stream: {}", int(state_),
            int(phase_), end_stream);

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterHeadersStatus::Continue;
  }

  ASSERT(phase_ == Phase::Init);
  phase_ = Phase::DecodeHeader;

  bool done = doHeaders(headers, end_stream);

  return done ? Http::FilterHeadersStatus::Continue : Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug,
            "golang filter decodeData, state: {}, phase: {}, data length: {}, end_stream: {}",
            int(state_), int(phase_), data.length(), end_stream);

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterDataStatus::Continue;
  }

  bool done = doData(data, end_stream);

  if (done) {
    data.move(do_data_buffer_);
    return Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationNoBuffer;
}

Http::FilterTrailersStatus Filter::decodeTrailers(Http::RequestTrailerMap& trailers) {
  ENVOY_LOG(debug, "golang filter decodeTrailers, state: {}, phase: {}", int(state_), int(phase_));

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterTrailersStatus::Continue;
  }

  bool done = doTrailer(trailers);

  return done ? Http::FilterTrailersStatus::Continue : Http::FilterTrailersStatus::StopIteration;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(debug, "golang filter encodeHeaders, state: {}, phase: {}, end_stream: {}", int(state_),
            int(phase_), end_stream);

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterHeadersStatus::Continue;
  }

  // NP: is safe to overwrite it since go code won't read it directly
  ENVOY_LOG(debug, "golang filter set data_buffer_ to nullptr since enter encodeHeaders");
  data_buffer_ = nullptr;

  // NP: may enter encodeHeaders in any phase & any state_,
  // since other filters or filtermanager could call encodeHeaders or sendLocalReply in any time.
  // eg. filtermanager may invoke sendLocalReply, when scheme is invalid,
  // with "Sending local reply with details // http1.invalid_scheme" details.
  if (phase_ != Phase::EncodeHeader) {
    ENVOY_LOG(warn,
              "golang filter enter encodeHeaders early, maybe sendLocalReply or encodeHeaders "
              "happened, current state: {}, phase: {}",
              int(state_), int(phase_));

    // get the state before changing it.
    bool inGo = isDoInGo();

    // it's safe to reset phase_ and state_, since they are read/write in safe thread.
    ENVOY_LOG(debug, "golang filter phase grow to EncodeHeader and state grow to WaitHeader since "
                     "enter encodeHeaders early");
    phase_ = Phase::EncodeHeader;
    state_ = FilterState::WaitHeader;

    if (inGo) {
      // NP: wait go returns to avoid concurrency conflict in go side.
      // also, can not drain do_data_buffer_, since it may be using.
      local_reply_waiting_go_ = true;
      ENVOY_LOG(debug, "waiting go returns before handle the local reply from other filter");

      // NP: save to another local_headers_ variable to avoid conflict,
      // since the headers_ may be used in Go side.
      local_headers_ = &headers;

      // can not use "StopAllIterationAndWatermark" here, since Go decodeHeaders may return
      // stopAndBuffer, that means it need data buffer and not continue header.
      return Http::FilterHeadersStatus::StopIteration;

    } else {
      if (do_data_buffer_.length() > 0) {
        ENVOY_LOG(warn, "golang filter drain do_data_buffer_ before continue encodeHeader, "
                        "since no go code is running");
        do_data_buffer_.drain(do_data_buffer_.length());
      }
    }
  }

  bool done = doHeaders(headers, end_stream);

  return done ? Http::FilterHeadersStatus::Continue : Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug,
            "golang filter encodeData, state: {}, phase: {}, data length: {}, end_stream: {}",
            int(state_), int(phase_), data.length(), end_stream);

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterDataStatus::Continue;
  }

  if (local_reply_waiting_go_) {
    ENVOY_LOG(debug, "golang filter appending data to buffer");
    if (data_buffer_ == nullptr) {
      data_buffer_ = createWatermarkBuffer();
    }
    data_buffer_->move(data);
    end_stream_ = end_stream;
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }

  bool done = doData(data, end_stream);

  if (done) {
    data.move(do_data_buffer_);
    return Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationNoBuffer;
}

Http::FilterTrailersStatus Filter::encodeTrailers(Http::ResponseTrailerMap& trailers) {
  ENVOY_LOG(debug, "golang filter encodeTrailers, state: {}, phase: {}", int(state_), int(phase_));

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterTrailersStatus::Continue;
  }

  if (local_reply_waiting_go_) {
    // NP: save to another local_trailers_ variable to avoid conflict,
    // since the trailers_ may be used in Go side.
    local_trailers_ = &trailers;
    return Http::FilterTrailersStatus::StopIteration;
  }

  bool done = doTrailer(trailers);

  return done ? Http::FilterTrailersStatus::Continue : Http::FilterTrailersStatus::StopIteration;
}

void Filter::onDestroy() {
  ENVOY_LOG(info, "golang filter on destroy");

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (has_destroyed_) {
      ENVOY_LOG(warn, "golang filter has been destroyed");
      return;
    }
    has_destroyed_ = true;
  }

  if (dynamicLib_ == NULL) {
    ENVOY_LOG(error, "golang filter dynamicLib is nullPtr.");
    return;
  }

  try {
    ASSERT(req_ != nullptr);
    auto reason = (state_ == FilterState::DoHeader || state_ == FilterState::DoData)
                      ? DestroyReason::Terminate
                      : DestroyReason::Normal;

    dynamicLib_->moeOnHttpDestroy(req_, int(reason));
  } catch (...) {
    ENVOY_LOG(error, "golang filter onDestroy do destoryStream catch "
                     "unknown exception.");
  }
}

void Filter::onStreamComplete() {
  addGolangMetadata("cost_decode", cost_time_decode_);
  addGolangMetadata("cost_encode", cost_time_encode_);
  addGolangMetadata("cost_total", cost_time_decode_ + cost_time_encode_ + cost_time_mem_);
}

void Filter::addGolangMetadata(const std::string& k, const uint64_t v) {
  ProtobufWkt::Struct value;
  ProtobufWkt::Value val;
  val.set_number_value(static_cast<double>(v));
  (*value.mutable_fields())[k] = val;
  decoder_callbacks_->streamInfo().setDynamicMetadata("golang.extension", value);
}

// access_log is executed before the log of the stream filter
void Filter::log(const Http::RequestHeaderMap*, const Http::ResponseHeaderMap*,
                 const Http::ResponseTrailerMap*, const StreamInfo::StreamInfo&) {
  // Todo log phase of stream filter
}

/*** status, state, phase ***/

bool Filter::handleGolangStatusInHeader(GolangStatus status) {
  ENVOY_LOG(debug, "golang filter handle header status, state: {}, phase: {}, status: {}",
            int(state_), int(phase_), int(status));

  switch (status) {
  case GolangStatus::LocalReply:
    // already invoked sendLocalReply, do nothing
    break;

  case GolangStatus::Running:
    // do nothing, go side turn to async mode
    break;

  case GolangStatus::Continue:
    if (do_end_stream_) {
      phaseGrow(3);
      state_ = FilterState::Done;
    } else {
      phaseGrow();
      state_ = FilterState::WaitData;
    }
    // NP: we should read/write headers now, since headers will pass to next fitler.
    headers_ = nullptr;
    return true;

  case GolangStatus::StopAndBuffer:
    phaseGrow();
    state_ = FilterState::WaitFullData;
    break;

  case GolangStatus::StopAndBufferWatermark:
    phaseGrow();
    state_ = FilterState::WaitData;
    break;

  default:
    // TODO: terminate the stream?
    ENVOY_LOG(error, "unexpected status: {}", int(status));
    break;
  }

  ENVOY_LOG(debug, "golang filter after handle header status, state: {}, phase: {}, status: {}",
            int(state_), int(phase_), int(status));

  return false;
}

bool Filter::handleGolangStatusInData(GolangStatus status) {
  ENVOY_LOG(debug, "golang filter handle data status, state: {}, phase: {}, status: {}",
            int(state_), int(phase_), int(status));

  bool done = false;

  switch (status) {
  case GolangStatus::LocalReply:
    // already invoked sendLocalReply, do nothing
    // return directly to skip phase grow by checking trailers
    return false;

  case GolangStatus::Running:
    // do nothing, go side turn to async mode
    // return directly to skip phase grow by checking trailers
    return false;

  case GolangStatus::Continue:
    if (do_end_stream_) {
      phaseGrow(2);
      state_ = FilterState::Done;
    } else {
      state_ = FilterState::WaitData;
    }
    done = true;
    break;

  case GolangStatus::StopAndBuffer:
    if (do_end_stream_) {
      ENVOY_LOG(error, "want more data while stream is end");
      // TODO: terminate the stream?
    }
    state_ = FilterState::WaitFullData;
    break;

  case GolangStatus::StopAndBufferWatermark:
    if (do_end_stream_) {
      ENVOY_LOG(error, "want more data while stream is end");
      // TODO: terminate the stream?
    }
    state_ = FilterState::WaitData;
    break;

  case GolangStatus::StopNoBuffer:
    if (do_end_stream_) {
      ENVOY_LOG(error, "want more data while stream is end");
      // TODO: terminate the stream?
    }
    do_data_buffer_.drain(do_data_buffer_.length());
    state_ = FilterState::WaitData;
    break;

  default:
    // TODO: terminate the stream?
    ENVOY_LOG(error, "unexpected status: {}", int(status));
    break;
  }

  // see trailers and not buffered data
  if (trailers_ != nullptr && isEmptyBuffer()) {
    ENVOY_LOG(error, "see trailers and buffer is empty");
    phaseGrow();
    state_ = FilterState::WaitTrailer;
  }

  ENVOY_LOG(debug, "golang filter after handle data status, state: {}, phase: {}, status: {}",
            int(state_), int(phase_), int(status));

  return done;
}

bool Filter::handleGolangStatusInTrailer(GolangStatus status) {
  ENVOY_LOG(debug, "golang filter handle trailer status, state: {}, phase: {}, status: {}",
            int(state_), int(phase_), int(status));

  switch (status) {
  case GolangStatus::LocalReply:
    // already invoked sendLocalReply, do nothing
    break;

  case GolangStatus::Running:
    // do nothing, go side turn to async mode
    break;

  case GolangStatus::Continue:
    phaseGrow();
    state_ = FilterState::Done;

    // NP: we should read/write trailers now, since trailers will pass to next fitler.
    trailers_ = nullptr;
    return true;

  default:
    // TODO: terminate the stream?
    ENVOY_LOG(error, "unexpected status: {}", int(status));
    break;
  }

  ENVOY_LOG(debug, "golang filter after handle trailer status, state: {}, phase: {}, status: {}",
            int(state_), int(phase_), int(status));

  return false;
}

// must in envoy thread.
bool Filter::handleGolangStatus(GolangStatus status) {
  ASSERT(isThreadSafe());
  ASSERT(state_ == FilterState::DoHeader || state_ == FilterState::DoData ||
             state_ == FilterState::DoTrailer,
         "unexpected state");

  ENVOY_LOG(debug,
            "before handle golang status, status: {}, state: {}, phase: {}, end_stream: {}, "
            "do_end_stream_: {}",
            int(status), int(state_), int(phase_), end_stream_, do_end_stream_);

  bool done = false;
  switch (state_) {
  case FilterState::DoHeader:
    done = handleGolangStatusInHeader(status);
    break;

  case FilterState::DoData:
    done = handleGolangStatusInData(status);
    break;

  case FilterState::DoTrailer:
    done = handleGolangStatusInTrailer(status);
    break;

  default:
    ASSERT(0, "unexpected state");
  }

  ENVOY_LOG(debug,
            "after handle golang status, status: {}, state: {}, phase: {}, end_stream: {}, "
            "do_end_stream_: {}",
            int(status), int(state_), int(phase_), end_stream_, do_end_stream_);

  return done;
}

/*** watermark buffer in golang filter side ***/

Buffer::InstancePtr Filter::createWatermarkBuffer() {
  if (isDecodePhase()) {
    auto buffer = getDispatcher().getWatermarkFactory().createBuffer(
        [this]() -> void {
          ENVOY_LOG(debug, "golang filter decode data buffer want more data");
          decoder_callbacks_->onDecoderFilterBelowWriteBufferLowWatermark();
        },
        [this]() -> void {
          if (state_ == FilterState::WaitFullData) {
            // On the request path exceeding buffer limits will result in a 413.
            ENVOY_LOG(debug, "golang filter decode data buffer is full, reply with 413");
            decoder_callbacks_->sendLocalReply(
                Http::Code::PayloadTooLarge,
                Http::CodeUtility::toString(Http::Code::PayloadTooLarge), nullptr, absl::nullopt,
                StreamInfo::ResponseCodeDetails::get().RequestPayloadTooLarge);
            return;
          }
          ENVOY_LOG(debug, "golang filter decode data buffer is full, disable reading");
          decoder_callbacks_->onDecoderFilterAboveWriteBufferHighWatermark();
        },
        []() -> void { /* TODO: Handle overflow watermark */ });
    buffer->setWatermarks(decoder_callbacks_->decoderBufferLimit());
    return buffer;

  } else {
    auto buffer = getDispatcher().getWatermarkFactory().createBuffer(
        [this]() -> void {
          ENVOY_LOG(debug, "golang filter encode data buffer want more data");
          encoder_callbacks_->onEncoderFilterBelowWriteBufferLowWatermark();
        },
        [this]() -> void {
          if (state_ == FilterState::WaitFullData) {
            // On the response path exceeding buffer limits will result in a 500.
            ENVOY_LOG(debug, "golang filter encode data buffer is full, reply with 500");
            encoder_callbacks_->sendLocalReply(
                Http::Code::InternalServerError,
                Http::CodeUtility::toString(Http::Code::InternalServerError), nullptr,
                absl::nullopt, StreamInfo::ResponseCodeDetails::get().ResponsePayloadTooLarge);
            return;
          }
          ENVOY_LOG(debug, "golang filter encode data buffer is full, disable reading");
          encoder_callbacks_->onEncoderFilterAboveWriteBufferHighWatermark();
        },
        []() -> void { /* TODO: Handle overflow watermark */ });
    buffer->setWatermarks(encoder_callbacks_->encoderBufferLimit());
    return buffer;
  }
}

/*** common APIs for filter, both decode and encode ***/

GolangStatus Filter::doHeadersGo(Http::RequestOrResponseHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(debug, "golang filter passing headers to golang, state: {}, phase: {}", int(state_),
            int(phase_));

  // may be EncodeHeader phase when previous filter trigger sendLocalReply
  // that will skip DecodeHeader in go.
  ASSERT(phase_ == Phase::DecodeHeader || phase_ == Phase::EncodeHeader);
  state_ = FilterState::DoHeader;

  try {
    if (req_ == nullptr) {
      httpRequestInternal* req = new httpRequestInternal(weak_from_this());
      req_ = reinterpret_cast<httpRequest*>(req);
      req_->configId = config_->getConfigId();
    }

    req_->phase = int(phase_);
    headers_ = &headers;
    do_end_stream_ = end_stream;
    auto status =
        dynamicLib_->moeOnHttpHeader(req_, end_stream ? 1 : 0, headers.size(), headers.byteSize());
    return static_cast<GolangStatus>(status);

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter doHeadersGo catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter doHeadersGo catch unknown exception.");
  }

  return GolangStatus::Continue;
}

bool Filter::doHeaders(Http::RequestOrResponseHeaderMap& headers, bool end_stream) {
  ASSERT(isEmptyBuffer());
  end_stream_ = end_stream;

  auto status = doHeadersGo(headers, end_stream);
  return handleGolangStatusInHeader(static_cast<GolangStatus>(status));
}

bool Filter::doDataGo(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug, "golang filter passing data to golang, state: {}, phase: {}", int(state_),
            int(phase_));

  ASSERT(state_ == FilterState::WaitData ||
         (state_ == FilterState::WaitFullData && (end_stream || trailers_ != nullptr)));

  state_ = FilterState::DoData;

  // FIXME: use a buffer list, for more flexible API in go side.
  do_data_buffer_.move(data);

  try {
    ASSERT(req_ != nullptr);
    req_->phase = int(phase_);
    do_end_stream_ = end_stream;
    auto status = dynamicLib_->moeOnHttpData(req_, end_stream ? 1 : 0,
                                             reinterpret_cast<uint64_t>(&do_data_buffer_),
                                             do_data_buffer_.length());
    return handleGolangStatusInData(static_cast<GolangStatus>(status));

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter decodeData catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter decodeData catch unknown exception.");
  }

  return false;
}

bool Filter::doData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(debug, "golang filter doData, state: {}, phase: {}", int(state_), int(phase_));
  end_stream_ = end_stream;

  bool done = false;
  switch (state_) {
  case FilterState::WaitData:
    done = doDataGo(data, end_stream);
    break;
  case FilterState::WaitFullData:
    if (end_stream) {
      if (!isEmptyBuffer()) {
        // NP: new data = data_buffer_ + data
        data_buffer_->move(data);
        data.move(*data_buffer_);
      }
      // check state again since data_buffer may be full and sendLocalReply with 413.
      if (state_ == FilterState::WaitFullData) {
        done = doDataGo(data, end_stream);
      }
      break;
    }
    // NP: not break, continue
    [[fallthrough]];
  case FilterState::DoData:
  case FilterState::DoHeader:
    if (data_buffer_ == nullptr) {
      data_buffer_ = createWatermarkBuffer();
    }
    ENVOY_LOG(debug, "golang filter appending data to buffer");
    data_buffer_->move(data);
    break;
  default:
    ENVOY_LOG(error, "unexpected state: {}", int(state_));
    // TODO: terminate stream?
    break;
  }

  ENVOY_LOG(debug, "golang filter doData, return: {}", done);

  return done;
}

bool Filter::doTrailerGo(Http::HeaderMap& trailers) {
  ENVOY_LOG(error, "golang filter passing trailers to golang, state: {}, phase: {}", int(state_),
            int(phase_));

  ASSERT(phase_ == Phase::DecodeTrailer || phase_ == Phase::EncodeTrailer);
  ASSERT(state_ == FilterState::WaitTrailer || state_ == FilterState::WaitData ||
         state_ == FilterState::WaitFullData);

  state_ = FilterState::DoTrailer;

  bool done = true;
  try {
    ASSERT(req_ != nullptr);

    req_->phase = int(phase_);
    auto status = dynamicLib_->moeOnHttpHeader(req_, 1, trailers.size(), trailers.byteSize());
    done = handleGolangStatusInTrailer(static_cast<GolangStatus>(status));

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter doTrailer catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter doTrailer catch unknown exception.");
  }
  return done;
}

bool Filter::doTrailer(Http::HeaderMap& trailers) {
  ENVOY_LOG(debug, "golang filter doTrailer, state: {}, phase: {}", int(state_), int(phase_));

  ASSERT(end_stream_ == false && do_end_stream_ == false);

  trailers_ = &trailers;

  bool done = false;
  switch (state_) {
  case FilterState::WaitTrailer:
    done = doTrailerGo(trailers);
    break;
  case FilterState::WaitData:
    phaseGrow();
    done = doTrailerGo(trailers);
    break;
  case FilterState::WaitFullData:
    ENVOY_LOG(debug, "golang filter data buffer is empty: {}", isEmptyBuffer());
    // do data first
    if (!isEmptyBuffer()) {
      done = doDataGo(*data_buffer_, false);
    }
    // NP: can not use done as condition here, since done will be false
    // maybe we can remove the done variable totally? by using state_ only?
    // continue trailers
    if (state_ == FilterState::WaitTrailer) {
      done = doTrailerGo(trailers);
    }
    break;
  case FilterState::DoData:
  case FilterState::DoHeader:
    // do nothing, wait previous task
    break;
  default:
    ENVOY_LOG(error, "unexpected state: {}", int(state_));
    // TODO: terminate stream?
    break;
  }

  ENVOY_LOG(debug, "golang filter doTrailer, return: {}", done);

  return done;
}

/*** APIs for go call C ***/

void Filter::commonContinue(bool is_decode) {
  if (is_decode) {
    ENVOY_LOG(debug, "golang filter callback continue, continueDecoding");
    decoder_callbacks_->continueDecoding();
  } else {
    ENVOY_LOG(debug, "golang filter callback continue, continueEncoding");
    encoder_callbacks_->continueEncoding();
  }
}

void Filter::continueData(bool is_decode) {
  // end_stream_ should be false, when trailers_ is not nullptr.
  if (!end_stream_ && do_data_buffer_.length() == 0) {
    return;
  }
  Buffer::OwnedImpl data_to_write;
  data_to_write.move(do_data_buffer_);
  if (is_decode) {
    ENVOY_LOG(debug, "golang filter callback continue, injectDecodedDataToFilterChain");
    decoder_callbacks_->injectDecodedDataToFilterChain(data_to_write, do_end_stream_);
  } else {
    ENVOY_LOG(debug, "golang filter callback continue, injectEncodedDataToFilterChain");
    encoder_callbacks_->injectEncodedDataToFilterChain(data_to_write, do_end_stream_);
  }
}

void Filter::continueEncodeLocalReply() {
  ENVOY_LOG(
      debug,
      "golang filter continue encodeHeader(local reply from other filters) after return from go");

  if (do_data_buffer_.length() > 0) {
    ENVOY_LOG(warn, "golang filter drain do_data_buffer_ before continueEncodeLocalReply");
    do_data_buffer_.drain(do_data_buffer_.length());
  }

  local_reply_waiting_go_ = false;

  auto header_end_stream = end_stream_;
  if (local_trailers_ != nullptr) {
    trailers_ = local_trailers_;
    header_end_stream = false;
  }
  if (!isEmptyBuffer()) {
    header_end_stream = false;
  }
  // NP: not overwrite end_stream_ in doHeadersGo
  auto status = doHeadersGo(*local_headers_, header_end_stream);

  continueStatusInternal(status);
}

void Filter::continueStatusInternal(GolangStatus status) {
  ASSERT(isThreadSafe());
  auto is_decode = isDecodePhase();
  auto saved_state = state_;

  if (local_reply_waiting_go_) {
    ENVOY_LOG(warn,
              "other filter already trigger sendLocalReply, ignoring the continue status: {}, "
              "state: {}, phase: {}",
              int(status), int(state_), int(phase_));

    continueEncodeLocalReply();
    return;
  }

  auto done = handleGolangStatus(status);
  if (done) {
    switch (saved_state) {
    case FilterState::DoHeader:
      // NP: should process data first filter seen the stream is end but go doesn't,
      // otherwise, the next filter will continue with end_stream = true.

      // NP: it is safe to continueData after commonContinue
      // that means injectDecodedDataToFilterChain after continueDecoding while stream is not end
      if (do_end_stream_ || (!end_stream_ && trailers_ == nullptr)) {
        commonContinue(is_decode);
      }
      break;

    case FilterState::DoData:
      continueData(is_decode);
      break;

    case FilterState::DoTrailer:
      if (do_data_buffer_.length() > 0) {
        continueData(is_decode);
      }
      commonContinue(is_decode);
      break;

    default:
      ASSERT(0, "unexpected state");
    }
  }

  if ((state_ == FilterState::WaitData && (!isEmptyBuffer() || end_stream_)) ||
      (state_ == FilterState::WaitFullData && (end_stream_ || trailers_ != nullptr))) {
    auto done = doDataGo(*data_buffer_, end_stream_);
    if (done) {
      continueData(is_decode);
    } else {
      // do not process trailers when data is not finished
      return;
    }
  }

  if (state_ == FilterState::WaitTrailer && trailers_ != nullptr) {
    auto done = doTrailerGo(*trailers_);
    if (done) {
      commonContinue(is_decode);
    }
  }
}

void Filter::sendLocalReplyInternal(
    Http::Code response_code, absl::string_view body_text,
    std::function<void(Http::ResponseHeaderMap& headers)> modify_headers,
    Grpc::Status::GrpcStatus grpc_status, absl::string_view details) {
  ENVOY_LOG(debug, "sendLocalReply Internal, response code: {}", int(response_code));

  if (local_reply_waiting_go_) {
    ENVOY_LOG(warn,
              "other filter already invoked sendLocalReply or encodeHeaders, ignoring the local "
              "reply from go, code: {}, body: {}, details: {}",
              int(response_code), body_text, details);

    continueEncodeLocalReply();
    return;
  }

  if (do_data_buffer_.length() > 0) {
    ENVOY_LOG(warn, "golang filter drain do_data_buffer_ before sendLocalReply");
    do_data_buffer_.drain(do_data_buffer_.length());
  }

  if (!isEmptyBuffer()) {
    ENVOY_LOG(warn, "golang filter drain data_buffer_ before sendLocalReply");
    data_buffer_->drain(data_buffer_->length());
  }

  if (isDecodePhase()) {
    // it's safe to reset phase_ and state_, since they are read/write in safe thread.
    ENVOY_LOG(debug, "golang filter phase grow to EncodeHeader and state grow to WaitHeader before "
                     "sendLocalReply");
    phase_ = Phase::EncodeHeader;
    state_ = FilterState::WaitHeader;

    decoder_callbacks_->sendLocalReply(response_code, body_text, modify_headers, grpc_status,
                                       details);
  } else {
    encoder_callbacks_->sendLocalReply(response_code, body_text, modify_headers, grpc_status,
                                       details);
  }
}

void Filter::sendLocalReply(Http::Code response_code, absl::string_view body_text,
                            std::function<void(Http::ResponseHeaderMap& headers)> modify_headers,
                            Grpc::Status::GrpcStatus grpc_status, absl::string_view details) {
  ENVOY_LOG(debug, "sendLocalReply, response code: {}", int(response_code));

  auto weak_ptr = weak_from_this();
  getDispatcher().post(
      [this, weak_ptr, response_code, body_text, modify_headers, grpc_status, details] {
        ASSERT(isThreadSafe());
        // do not need lock here, since it's the work thread now.
        if (!weak_ptr.expired() && !has_destroyed_) {
          sendLocalReplyInternal(response_code, body_text, modify_headers, grpc_status, details);
        } else {
          ENVOY_LOG(info, "golang filter has gone or destroyed in sendLocalReply");
        }
      });
};

void Filter::continueStatus(GolangStatus status) {
  // TODO: skip post event to dispatcher, and return continue in the caller,
  // when it's invoked in the current envoy thread, for better performance & latency.
  ENVOY_LOG(info, "golang filter continue from Go, status: {}, state_: {}, phase_: {}", int(status),
            int(state_), int(phase_));

  auto weak_ptr = weak_from_this();
  getDispatcher().post([this, weak_ptr, status] {
    ASSERT(isThreadSafe());
    // do not need lock here, since it's the work thread now.
    if (!weak_ptr.expired() && !has_destroyed_) {
      continueStatusInternal(status);
    } else {
      ENVOY_LOG(info, "golang filter has gone or destroyed in continueStatus event");
    }
  });
}

absl::optional<absl::string_view> Filter::getHeader(absl::string_view key) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return "";
  }
  auto m = isHeaderPhase() ? headers_ : trailers_;
  auto result = m->get(Http::LowerCaseString(key));

  if (result.empty()) {
    return absl::nullopt;
  }
  return result[0]->value().getStringView();
}

void copyHeaderMapToGo(Http::HeaderMap& m, GoString* goStrs, char* goBuf) {
  auto i = 0;
  m.iterate([&i, &goStrs, &goBuf](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    auto key = std::string(header.key().getStringView());
    auto value = std::string(header.value().getStringView());

    // std::cout << "idx: " << i << ", key: " << key << ", value: " << value << std::endl;

    auto len = key.length();
    goStrs[i].n = len;
    goStrs[i].p = goBuf;
    memcpy(goBuf, key.data(), len);
    goBuf += len;
    i++;

    len = value.length();
    goStrs[i].n = len;
    goStrs[i].p = goBuf;
    memcpy(goBuf, value.data(), len);
    goBuf += len;
    i++;
    return Http::HeaderMap::Iterate::Continue;
  });
}

void Filter::copyHeaders(GoString* goStrs, char* goBuf) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  ASSERT(headers_ != nullptr, "headers is empty, may already continue to next filter");
  copyHeaderMapToGo(*headers_, goStrs, goBuf);
}

void Filter::setHeader(absl::string_view key, absl::string_view value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  ASSERT(headers_ != nullptr, "headers is empty, may already continue to next filter");
  headers_->setCopy(Http::LowerCaseString(key), value);
}

void Filter::removeHeader(absl::string_view key) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  ASSERT(headers_ != nullptr, "headers is empty, may already continue to next filter");
  headers_->remove(Http::LowerCaseString(key));
}

void Filter::copyBuffer(Buffer::Instance* buffer, char* data) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  for (const Buffer::RawSlice& slice : buffer->getRawSlices()) {
    memcpy(data, static_cast<const char*>(slice.mem_), slice.len_);
    data += slice.len_;
  }
}

void Filter::setBuffer(Buffer::Instance* buffer, absl::string_view& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  buffer->drain(buffer->length());
  buffer->add(value);
}

void Filter::copyTrailers(GoString* goStrs, char* goBuf) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  ASSERT(trailers_ != nullptr, "trailers is empty");
  copyHeaderMapToGo(*trailers_, goStrs, goBuf);
}

void Filter::setTrailer(absl::string_view key, absl::string_view value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  ASSERT(trailers_ != nullptr, "trailers is empty");
  trailers_->setCopy(Http::LowerCaseString(key), value);
}

void Filter::getStringValue(int id, GoString* valueStr) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  // string will copy to req->strValue, but not deep copy
  auto req = reinterpret_cast<httpRequestInternal*>(req_);
  switch (static_cast<StringValue>(id)) {
  case StringValue::RouteName:
    // TODO: add a callback wrapper to avoid this kind of ugly code
    if (isDecodePhase()) {
      req->strValue = decoder_callbacks_->streamInfo().getRouteName();
    } else {
      req->strValue = encoder_callbacks_->streamInfo().getRouteName();
    }
    break;
  default:
    ASSERT(false, "invalid string value id");
  }

  valueStr->p = req->strValue.data();
  valueStr->n = req->strValue.length();
}

/*** FilterConfig ***/

FilterConfig::FilterConfig(const envoy::extensions::filters::http::golang::v3::Config& proto_config)
    : plugin_name_(proto_config.plugin_name()), so_id_(proto_config.so_id()),
      plugin_config_(proto_config.plugin_config()) {
  ENVOY_LOG(info, "initilizing golang filter config");
  // NP: dso may not loaded yet, can not invoke moeNewHttpPluginConfig yet.
};

uint64_t FilterConfig::getConfigId() {
  if (config_id_ != 0) {
    return config_id_;
  }
  auto dlib = Dso::DsoInstanceManager::getDsoInstanceByID(so_id_);
  if (dlib == NULL) {
    ENVOY_LOG(error, "golang extension filter dynamicLib is nullPtr.");
    return 0;
  }

  std::string str;
  if (!plugin_config_.SerializeToString(&str)) {
    ENVOY_LOG(error, "failed to serialize any pb to string");
    return 0;
  }
  auto ptr = reinterpret_cast<unsigned long long>(str.data());
  auto len = str.length();
  config_id_ = dlib->moeNewHttpPluginConfig(ptr, len);
  if (config_id_ == 0) {
    ENVOY_LOG(error, "invalid golang plugin config");
  }
  return config_id_;
}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

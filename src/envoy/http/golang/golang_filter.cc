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

void Filter::onHeadersModified() {
  // Any changes to request headers can affect how the request is going to be
  // routed. If we are changing the headers we also need to clear the route
  // cache.
  decoder_callbacks_->clearRouteCache();
}

void Filter::directResponse(Http::ResponseHeaderMapPtr&& headers, Buffer::Instance* body,
                            Http::ResponseTrailerMapPtr&& trailers) {
  decoder_callbacks_->encodeHeaders(std::move(headers),
                                    (body == nullptr && trailers.get()->empty()),
                                    "golang extension direct response");
  if (body) {
    decoder_callbacks_->encodeData(*body, (trailers == nullptr || trailers.get()->empty()));
  }

  if (!trailers.get()->empty()) {
    decoder_callbacks_->encodeTrailers(std::move(trailers));
  }
}

Event::Dispatcher& Filter::getDispatcher() { return decoder_callbacks_->dispatcher(); }

bool Filter::hasDestroyed() { return has_destroyed_; }

void Filter::continueHeader(bool is_decode) {
  if (is_decode) {
    ENVOY_LOG(debug, "golang filter callback continue, continueDecoding");
    decoder_callbacks_->continueDecoding();
  } else {
    ENVOY_LOG(debug, "golang filter callback continue, continueEncoding");
    encoder_callbacks_->continueEncoding();
  }
}

void Filter::continueData(bool is_decode) {
  Buffer::OwnedImpl data_to_write;
  data_to_write.move(do_data_buffer_);
  if (is_decode) {
    ENVOY_LOG(debug, "golang filter callback continue, injectDecodedDataToFilterChain");
    decoder_callbacks_->injectDecodedDataToFilterChain(data_to_write, isEnd());
  } else {
    ENVOY_LOG(debug, "golang filter callback continue, injectEncodedDataToFilterChain");
    encoder_callbacks_->injectEncodedDataToFilterChain(data_to_write, isEnd());
  }
}

void Filter::continueStatusInternal(GolangStatus status) {
  ASSERT(isThreadSafe());
  ENVOY_LOG(debug, "golang filter callback continue, status: {}, state: {}, phase: {}", int(status), int(state_), int(phase_));
  auto is_decode = isDecodePhase();
  auto is_header = isHeaderPhase();
  auto done = handleGolangStatus(status);
  ENVOY_LOG(debug, "golang filter callback continue, status: {}, state: {}, phase: {}", int(status), int(state_), int(phase_));
  if (done) {
    // we should process data first when end_stream_ = true and do_end_stream_ = false,
    // otherwise, the next filter will continue with end_stream = true.
    if (is_header && (!end_stream_ || do_end_stream_)) {
      continueHeader(is_decode);
    } else if (do_data_buffer_.length() > 0) {
      continueData(is_decode);
    }
  }
  if ((state_ == FilterState::WaitData && (!isEmptyBuffer() || end_stream_))
      || (state_ == FilterState::WaitFullData && end_stream_))
  {
    auto done = doDataGo(*data_buffer_, end_stream_);
    if (done) {
      continueData(is_decode);
    }
  }
}

void Filter::continueStatus(GolangStatus status) {
  // TODO: skip post event to dispatcher, and return continue in the caller,
  // when it's invoked in the current envoy thread, for better performance & latency.
  auto weak_ptr = weak_from_this();
  getDispatcher().post([this, weak_ptr, status]{
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
  auto result = headers_->get(Http::LowerCaseString(key));

  if (result.empty()) {
    return absl::nullopt;
  }
  return result[0]->value().getStringView();
}

void Filter::copyHeaders(_GoString_ *goStrs, char *goBuf) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  auto i = 0;
  headers_->iterate([this, &i, &goStrs, &goBuf](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
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

void Filter::setHeader(absl::string_view key, absl::string_view value) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  headers_->setCopy(Http::LowerCaseString(key), value);
}

void Filter::copyBuffer(Buffer::Instance* buffer, char *data) {
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

bool Filter::isThreadSafe() {
  return decoder_callbacks_->dispatcher().isThreadSafe();
}

bool Filter::isDecodePhase() {
  return phase_ == Phase::DecodeHeader || phase_ == Phase::DecodeData;
}

bool Filter::isHeaderPhase() {
  return phase_ == Phase::DecodeHeader || phase_ == Phase::EncodeHeader;
}

bool Filter::isEmptyBuffer() {
  return data_buffer_ == nullptr || data_buffer_->length() == 0;
}

bool Filter::isEnd() {
  return end_stream_ && isEmptyBuffer();
}

// must in envoy thread.
bool Filter::handleGolangStatus(GolangStatus status) {
  ASSERT(isThreadSafe());
  ASSERT(state_ == FilterState::DoHeader || state_ == FilterState::DoData);

  auto is_end = isEnd();
  ENVOY_LOG(debug, "before handle golang status, status: {}, state: {}, phase: {}, end_stream: {}, is_end: {}", int(status), int(state_), int(phase_), end_stream_, is_end);

  bool done = false;
  switch (status) {
  case GolangStatus::Continue:
    state_ = is_end ? FilterState::Done : FilterState::WaitData;
    if (isHeaderPhase()) {
      ENVOY_LOG(debug, "phase continue since continue in header phase");
      phase_ = static_cast<Phase>(static_cast<int>(phase_) + 1);
    }
    if (do_end_stream_) {
      ENVOY_LOG(debug, "phase continue since stream end");
      phase_ = static_cast<Phase>(static_cast<int>(phase_) + 1);
    }
    done = true;
    break;

  case GolangStatus::StopAndBuffer:
    if (do_end_stream_) {
      ENVOY_LOG(error, "want more data in an ended stream");
      // TODO: terminate the stream?
    }
    state_ = FilterState::WaitFullData;
    break;

  case GolangStatus::StopAndBufferWatermark:
    if (do_end_stream_) {
      ENVOY_LOG(error, "want more data in an ended stream");
      // TODO: terminate the stream?
    }
    state_ = FilterState::WaitData;
    break;

  case GolangStatus::StopNoBuffer:
    if (do_end_stream_) {
      ENVOY_LOG(error, "want more data in an ended stream");
      // TODO: terminate the stream?
    }
    do_data_buffer_.drain(do_data_buffer_.length());
    state_ = FilterState::WaitData;
    break;

  case GolangStatus::Running:
    // keep state_
    break;
  
  default:
    // TODO: terminate the stream?
    ENVOY_LOG(error, "unexpected status: {}", int(status));
  }

  ENVOY_LOG(debug, "after handle golang status, status: {}, state: {}, phase: {}, end_stream: {}, is_end: {}", int(status), int(state_), int(phase_), end_stream_, is_end);

  return done;
}

bool Filter::doHeaders(Http::RequestOrResponseHeaderMap& headers, bool end_stream) {
  ASSERT(isHeaderPhase());
  ASSERT(data_buffer_ == nullptr || data_buffer_->length() == 0);

  if (req_ == nullptr) {
    ASSERT(phase_ == Phase::DecodeHeader);
    httpRequestInternal* req = new httpRequestInternal(weak_from_this());
    req_ = reinterpret_cast<httpRequest*>(req);
    req_->configId = config_->getConfigId();
  }

  state_ = FilterState::DoHeader;
  end_stream_ = end_stream;

  bool done = true;
  try {
    ASSERT(req_ != nullptr);

    req_->phase = int(phase_);
    headers_ = &headers;
    do_end_stream_ = end_stream;
    auto status = dynamicLib_->moeOnHttpHeader(req_, end_stream ? 1 : 0, headers.size(), headers.byteSize());
    done = handleGolangStatus(static_cast<GolangStatus>(status));

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter decodeHeaders catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter decodeHeaders catch unknown exception.");
  }
  return done;
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(info, "golang filter decodeHeaders, end_stream: {}.", end_stream);

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterHeadersStatus::Continue;
  }

  ASSERT(phase_ == Phase::Init);
  phase_ = Phase::DecodeHeader;

  // TODO: move phase, configId and ptrHolder to the request object.
  // auto req = initRequest();

  // headers.path();
  // headers.getByKey("foo");

  bool done = doHeaders(headers, end_stream);

  return done ? Http::FilterHeadersStatus::Continue : Http::FilterHeadersStatus::StopIteration;
}

void Filter::wantMoreData() {
  ENVOY_LOG(debug, "golang filter watermark buffer want more data");
  if (isDecodePhase()) {
    decoder_callbacks_->onDecoderFilterBelowWriteBufferLowWatermark();
  } else {
    encoder_callbacks_->onEncoderFilterBelowWriteBufferLowWatermark();
  }
}

void Filter::dataBufferFull() {
  ENVOY_LOG(debug, "golang filter watermark buffer is full");
  if (state_ == FilterState::WaitFullData) {
    // TODO: 413.
  }
  if (isDecodePhase()) {
    decoder_callbacks_->onDecoderFilterAboveWriteBufferHighWatermark();
  } else {
    encoder_callbacks_->onEncoderFilterAboveWriteBufferHighWatermark();
  }
}

Buffer::InstancePtr Filter::createWatermarkBuffer() {
  auto buffer = getDispatcher().getWatermarkFactory().createBuffer(
    [this]() -> void { this->wantMoreData(); },
    [this]() -> void { this->dataBufferFull(); },
    []() -> void { /* TODO: Handle overflow watermark */ });
  buffer->setWatermarks(decoder_callbacks_->decoderBufferLimit()); // TODO: encoderBufferLimit()
  return buffer;
}

bool Filter::doDataGo(Buffer::Instance& data, bool end_stream) {
  ASSERT(state_ == FilterState::WaitData || (state_ == FilterState::WaitFullData && end_stream));
  state_ = FilterState::DoData;

  // FIXME: use a buffer list, for more flexible API in go side.
  do_data_buffer_.move(data);

  try {
    ASSERT(req_ != nullptr);
    req_->phase = int(phase_);
    do_end_stream_ = end_stream;
    auto status = dynamicLib_->moeOnHttpData(req_, end_stream ? 1 : 0, reinterpret_cast<uint64_t>(&do_data_buffer_), do_data_buffer_.length());
    return handleGolangStatus(static_cast<GolangStatus>(status));

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter decodeData catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter decodeData catch unknown exception.");
  }

  return false;
}

bool Filter::doData(Buffer::Instance& data, bool end_stream) {
  end_stream_ = end_stream;

  bool done = false;
  switch (state_) {
  case FilterState::WaitData:
    done = doDataGo(data, end_stream);
    break;
  case FilterState::WaitFullData:
    if (end_stream) {
      if (data_buffer_ != nullptr) {
        data.move(*data_buffer_);
      }
      done = doDataGo(data, end_stream);
    }
    break;
  case FilterState::DoData:
  case FilterState::DoHeader:
    if (data_buffer_ == nullptr) {
      data_buffer_ = createWatermarkBuffer();
    }
    data_buffer_->move(data);
    break;
  default:
    ENVOY_LOG(error, "unexpected state: {}", int(state_));
    // TODO: terminate stream?
    break;
  }
  return done;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(error, "golang filter decodeData, end_stream: {}", end_stream);

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
  if (decode_goextension_executed_) {
    return Http::FilterTrailersStatus::Continue;
  }

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterTrailersStatus::Continue;
  }

  try {
    // TODO
  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang extension filter decodeTrailers catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang extension filter decodeTrailers catch unknown exception.");
  }

  return Http::FilterTrailersStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(info, "golang filter encodeHeaders, end_stream: {}.", end_stream);

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterHeadersStatus::Continue;
  }

  headers_ = &headers;

  ASSERT(phase_ == Phase::EncodeHeader);

  // headers.path();
  // headers.getByKey("foo");

  bool done = doHeaders(headers, end_stream);

  return done ? Http::FilterHeadersStatus::Continue : Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  ENVOY_LOG(error, "golang filter encodeData, end_stream: {}", end_stream);

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

Http::FilterTrailersStatus Filter::encodeTrailers(Http::ResponseTrailerMap& trailers) {
  if (encode_goextension_executed_) {
    return Http::FilterTrailersStatus::Continue;
  }

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterTrailersStatus::Continue;
  }

  try {
    // TODO
  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang extension filter encodeTrailers catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang extension filter encodeTrailers catch unknown exception.");
  }

  return Http::FilterTrailersStatus::Continue;
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
                  ? DestroyReason::Terminate : DestroyReason::Normal;

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

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

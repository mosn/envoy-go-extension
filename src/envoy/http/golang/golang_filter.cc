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
    // dynamicLib_->destoryStream(stream_id_, has_async_task_ ? 1 : 0);
  } catch (...) {
    ENVOY_LOG(error, "golang filter onDestroy do destoryStream catch "
                     "unknown exception.");
  }
}

/*
void Filter::buildHeadersOrTrailers(Http::HeaderMap& dheaders, NonConstString* sheaders) {
  // resp.headers ->
  // |{key0-data,key0-len}|{val0-data,val0-len}|,|{key1-data,key1-len}|{val1-data,val1-len}|null|
  for (int i = 0; sheaders[i].data != NULL;) {
    if (sheaders[i + 1].len != 0) {
      // overwrites any existing header, don't append it
      dheaders.setCopy(Http::LowerCaseString(std::string(sheaders[i].data, sheaders[i].len)),
                       absl::string_view(sheaders[i + 1].data, sheaders[i + 1].len));
    }

    i += 2;
  }
}
*/

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

std::string Filter::buildBody(const Buffer::Instance* buffered, const Buffer::Instance& last) {
  std::string body;
  body.reserve((buffered ? buffered->length() : 0) + last.length());
  if (buffered) {
    for (const Buffer::RawSlice& slice : buffered->getRawSlices()) {
      body.append(static_cast<const char*>(slice.mem_), slice.len_);
    }
  }

  for (const Buffer::RawSlice& slice : last.getRawSlices()) {
    body.append(static_cast<const char*>(slice.mem_), slice.len_);
  }

  return body;
}

/*
void Filter::buildGolangRequestBodyDecode(Request& req, const Buffer::Instance& data) {
  req_body_ = buildBody(decoder_callbacks_->decodingBuffer(), data);
  if (!req_body_.empty()) {
    // req_body.data save data pointer
    // req_body.len save data length
    req.req_body.data = req_body_.data();
    req.req_body.len = req_body_.length();
  }
}

void Filter::buildGolangRequestBodyEncode(Request& req, const Buffer::Instance& data) {
  resp_body_ = buildBody(encoder_callbacks_->encodingBuffer(), data);
  if (!resp_body_.empty()) {
    // req_body.data save data pointer
    // req_body.len save data length
    req.req_body.data = resp_body_.data();
    req.req_body.len = resp_body_.length();
  }
}

void Filter::buildGolangRequestFilterChain(Request& req, const std::string& filter_chain) {
  if (!filter_chain.empty()) {
    // filter_chain.data save data pointer
    // filter_chain.len save data length
    req.filter_chain.data = filter_chain.data();
    req.filter_chain.len = filter_chain.length();
  }
}

void Filter::buildGolangRequestRemoteAddress(Request& req) {
  // downstream
  const std::string& downStreamAddress = decoder_callbacks_->streamInfo()
                                             .downstreamAddressProvider()
                                             .directRemoteAddress()
                                             ->asString();
  if (!downStreamAddress.empty()) {
    req.downstream_address.data = downStreamAddress.data();
    req.downstream_address.len = downStreamAddress.length();
  }

  // upstream
  Upstream::HostDescriptionConstSharedPtr upstreamHost =
      encoder_callbacks_->streamInfo().upstreamInfo()->upstreamHost();
  if (upstreamHost != nullptr) {
    const std::string& upStreamAddress = upstreamHost->address()->asString();
    if (!upStreamAddress.empty()) {
      req.upstream_address.data = upStreamAddress.data();
      req.upstream_address.len = upStreamAddress.length();
    }
  }
}

bool Filter::buildGolangRequestHeaders(Request& req, Http::HeaderMap& headers) {
  if (req.headers == NULL) {
    ENVOY_LOG(error, "golang extension filter buildGolangRequestHeaders failed: "
                     "req.headers == NULL");
    return false;
  }

  buildGolangRequestFilterChain(req, config_->filter_chain());

  int i = 0;
  // initialize the CGO request with headers's reference key and val
  // req.headers ->
  // |key0-data|key0-len|val0-data|val0-len|,|key1-data|key1-len|val1-data|val1-len|null|
  headers.iterate([&req, &i](const Http::HeaderEntry& entry) -> Http::HeaderMap::Iterate {
    req.headers[i].data = entry.key().getStringView().data();
    req.headers[i].len = entry.key().getStringView().length();

    req.headers[i + 1].data = entry.value().getStringView().data();
    req.headers[i + 1].len = entry.value().getStringView().length();
    i += 2;
    return Http::HeaderMap::Iterate::Continue;
  });

  // init connection id and stream id
  req.cid = decoder_callbacks_->streamInfo().downstreamAddressProvider().connectionID().value();
  // Global increment ID, unique for each request.
  // The max value is 0xFFFFFFFFFFFFFFFF, So it can run for 5849424
  // (0xFFFFFFFFFFFFFFFF/(100000*86400.0*365.0)) years.
  req.sid = stream_id_;

  buildGolangRequestRemoteAddress(req);

  // save golang http filter for async callback
  req.filter = this;

  return true;
}

bool Filter::buildGolangRequestTrailers(Request& req, Http::HeaderMap& trailers) {
  if (req.trailers == NULL) {
    ENVOY_LOG(error, "golang extension filter buildGolangRequestTrailers failed: "
                     "req.trailers == NULL");
    return false;
  }

  int i = 0;
  // initialize the CGO request with trailers's reference key and val
  // req.trailers ->
  // |key0-data|key0-len|val0-data|val0-len|,|key1-data|key1-len|val1-data|val1-len|null|
  trailers.iterate([&req, &i](const Http::HeaderEntry& entry) -> Http::HeaderMap::Iterate {
    req.trailers[i].data = entry.key().getStringView().data();
    req.trailers[i].len = entry.key().getStringView().length();

    req.trailers[i + 1].data = entry.value().getStringView().data();
    req.trailers[i + 1].len = entry.value().getStringView().length();
    i += 2;
    return Http::HeaderMap::Iterate::Continue;
  });

  return true;
}
*/

Http::RequestHeaderMap* Filter::getRequestHeaders() { return request_headers_; }

Http::ResponseHeaderMap* Filter::getResponseHeaders() { return response_headers_; }

Event::Dispatcher& Filter::getDispatcher() { return decoder_callbacks_->dispatcher(); }

/*
void Filter::initReqAndResp(Request& req, Response&, size_t headerSize, size_t trailerSize) {
  req.headers = static_cast<ConstString*>(calloc(2 * headerSize + 1, sizeof(ConstString)));
  req.header_size = headerSize;

  req.trailers = static_cast<ConstString*>(calloc(2 * trailerSize + 1, sizeof(ConstString)));
  req.trailer_size = trailerSize;
}

void Filter::freeReqAndResp(Request& req, Response& resp) {
  // free req
  if (req.headers != nullptr) {
    free(req.headers);
  }
  if (req.trailers != nullptr) {
    free(req.trailers);
  }

  // free resp
  freeCharPointerArray(resp.headers);
  freeCharPointerArray(resp.trailers);
  if (resp.resp_body.data != nullptr) {
    free(resp.resp_body.data);
  }
}

void Filter::freeCharPointerArray(NonConstString* p) {
  if (p == nullptr) {
    return;
  }

  NonConstString* temp = p;
  for (; temp != nullptr && temp->data != nullptr; temp++) {
    free(temp->data);
  }

  free(p);
}
*/

bool Filter::hasDestroyed() { return has_destroyed_; }

void Filter::continueStatus(GolangStatus status) {
  // TODO: skip post event to dispatcher, and return continue in the caller,
  // when it's invoked in the current envoy thread, for better performance & latency.
  auto weak_ptr = weak_from_this();
  getDispatcher().post([this, weak_ptr, status]{
    ASSERT(isThreadSafe());
    // do not need lock here, since it's the work thread now.
    if (!weak_ptr.expired() && !has_destroyed_) {
      ENVOY_LOG(debug, "golang filter callback continue, status: {}, state: {}, phase: {}", int(status), int(state_), int(phase_));
      auto is_decode = isDecodePhase();
      auto is_header = isHeaderPhase();
      auto is_end = isEnd();
      auto done = handleGolangStatus(status);
      ENVOY_LOG(debug, "golang filter callback continue, status: {}, state: {}, phase: {}", int(status), int(state_), int(phase_));
      if (done) {
        // we should process data first when end_stream_ = true.
        if (is_header && !end_stream_) {
          if (is_decode) {
            ENVOY_LOG(debug, "golang filter callback continue, continueDecoding");
            decoder_callbacks_->continueDecoding();
          } else {
            ENVOY_LOG(debug, "golang filter callback continue, continueEncoding");
            encoder_callbacks_->continueEncoding();
          }
        } else {
          Buffer::OwnedImpl data_to_write;
          data_to_write.move(do_data_buffer_);
          if (is_decode) {
            ENVOY_LOG(debug, "golang filter callback continue, injectDecodedDataToFilterChain");
            decoder_callbacks_->injectDecodedDataToFilterChain(data_to_write, is_end);
          } else {
            ENVOY_LOG(debug, "golang filter callback continue, injectEncodedDataToFilterChain");
            encoder_callbacks_->injectEncodedDataToFilterChain(data_to_write, is_end);
          }
        }
      }
      if ((state_ == FilterState::WaitData && (!isEmptyBuffer() || end_stream_))
          || (state_ == FilterState::WaitFullData && end_stream_))
      {
        auto done = doDataGo(*data_buffer_, end_stream_);
        if (done) {
          Buffer::OwnedImpl data_to_write;
          data_to_write.move(do_data_buffer_);
          if (is_decode) {
            ENVOY_LOG(debug, "golang filter callback continue, injectDecodedDataToFilterChain, end_stream: {}", isEnd());
            decoder_callbacks_->injectDecodedDataToFilterChain(data_to_write, isEnd());
          } else {
            ENVOY_LOG(debug, "golang filter callback continue, injectEncodedDataToFilterChain, end_stream: {}", isEnd());
            encoder_callbacks_->injectEncodedDataToFilterChain(data_to_write, isEnd());
          }
        }
      }
    } else {
      ENVOY_LOG(info, "golang filter has gone or destroyed in continueStatus event");
    }
  });
}

absl::optional<absl::string_view> Filter::getRequestHeader(absl::string_view key) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return "";
  }
  return reinterpret_cast<Http::RequestHeaderMap*>(headers_)->getByKey(key);
}

void Filter::copyRequestHeaders(_GoString_ *goStrs, char *goBuf) {
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

void Filter::responseContinue() {
  // TODO: skip post event to dispatcher, and return continue in the caller,
  // when it's invoked in the current envoy thread, for better performance & latency.
  auto weak_ptr = weak_from_this();
  getDispatcher().post([this, weak_ptr]{
    // do not need lock here, since it's the work thread now.
    if (!weak_ptr.expired() && !has_destroyed_) {
      ASSERT(isThreadSafe());
      ENVOY_LOG(debug, "golang filter callback continueEncoding");
      encoder_callbacks_->continueEncoding();
    } else {
      ENVOY_LOG(info, "golang filter has gone or destroyed in requestContinue event");
    }
  });
}

absl::optional<absl::string_view> Filter::getResponseHeader(absl::string_view key) {
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

void Filter::copyResponseHeaders(_GoString_ *goStrs, char *goBuf) {
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

void Filter::setResponseHeader(absl::string_view key, absl::string_view value) {
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

bool Filter::handleGolangStatus(GolangStatus status) {
  ASSERT(state_ == FilterState::DoHeader || state_ == FilterState::DoData);

  auto is_end = isEnd();
  ENVOY_LOG(debug, "before handle golang status, status: {}, state: {}, phase: {}, end_stream: {}, is_end: {}", int(status), int(state_), int(phase_), end_stream_, is_end);

  bool done = false;
  switch (status) {
  case GolangStatus::Continue:
    state_ = is_end ? FilterState::Done : FilterState::WaitData;
    if (isHeaderPhase()) {
      phase_ = static_cast<Phase>(static_cast<int>(phase_) + 1);
    }
    if (is_end) {
      ENVOY_LOG(debug, "end_stream and no data buffer");
      phase_ = static_cast<Phase>(static_cast<int>(phase_) + 1);
    }
    done = true;
    break;

  case GolangStatus::StopAndBuffer:
    if (end_stream_) {
      // error
    }
    state_ = FilterState::WaitFullData;
    break;

  case GolangStatus::StopAndBufferWatermark:
    if (end_stream_) {
      // error
    }
    state_ = FilterState::WaitData;
    break;

  case GolangStatus::StopNoBuffer:
    if (end_stream_) {
      // error
    }
    do_data_buffer_.drain(do_data_buffer_.length());
    state_ = FilterState::WaitData;
    break;

  case GolangStatus::NeedAsync:
    // keep state_
    break;
  
  default:
    // error
    break;
  }

  ENVOY_LOG(debug, "after handle golang status, status: {}, state: {}, phase: {}, end_stream: {}, is_end: {}", int(status), int(state_), int(phase_), end_stream_, is_end);

  return done;
}

bool Filter::doHeaders(Http::RequestOrResponseHeaderMap& headers, bool end_stream) {
  ASSERT(isHeaderPhase());
  ASSERT(data_buffer_ == nullptr || data_buffer_->length() == 0);

  state_ = FilterState::DoHeader;
  end_stream_ = end_stream;

  bool done = true;
  try {
    auto id = config_->getConfigId();
    if (ptr_holder_ == 0) {
      FilterWeakPtrHolder* holder = new FilterWeakPtrHolder(weak_from_this());
      ptr_holder_ = reinterpret_cast<uint64_t>(holder);
    }
    headers_ = &headers;
    auto is_request = isDecodePhase() ? 1 : 0;
    auto status = dynamicLib_->moeOnHttpHeader(ptr_holder_, id, end_stream ? 1 : 0, is_request, headers.size(), headers.byteSize());
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

  phase_ = Phase::DecodeHeader;

  // headers.path();
  // headers.getByKey("foo");

  bool done = doHeaders(headers, end_stream);

  return done ? Http::FilterHeadersStatus::Continue : Http::FilterHeadersStatus::StopIteration;
}

void Filter::wantData() {
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
    [this]() -> void { this->wantData(); },
    [this]() -> void { this->dataBufferFull(); },
    []() -> void { /* TODO: Handle overflow watermark */ });
  buffer->setWatermarks(decoder_callbacks_->decoderBufferLimit()); // TODO: encoderBufferLimit()
  return buffer;
}

bool Filter::doDataGo(Buffer::Instance& data, bool end_stream) {
  ASSERT(state_ == FilterState::WaitData || (state_ == FilterState::WaitFullData && end_stream));
  state_ = FilterState::DoData;

  // TODO: use a buffer list, for more flexible API in go side.
  do_data_buffer_.move(data);

  try {
    ASSERT(ptr_holder_ != 0);
    auto id = config_->getConfigId();
    auto is_request = isDecodePhase() ? 1 : 0;
    auto status = dynamicLib_->moeOnHttpData(ptr_holder_, id, end_stream ? 1 : 0, is_request, reinterpret_cast<uint64_t>(&do_data_buffer_), do_data_buffer_.length());
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
    // die.
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
    /*
    Request req{};
    Response resp{};
    // create request and response struct
    cost_time_mem_ += measure<>::execution(
        [&]() { initReqAndResp(req, resp, request_headers_->size(), trailers.size()); });

    // build golang request, if the build fails, then the filter should be
    // skipped
    if (!buildGolangRequestHeaders(req, *request_headers_)) {
      // free memory
      cost_time_mem_ += measure<>::execution([&]() { freeReqAndResp(req, resp); });
      return Http::FilterTrailersStatus::Continue;
    }

    // build golang request body
    Buffer::OwnedImpl empty;
    buildGolangRequestBodyDecode(req, empty);

    // build golang request trailers
    if (!buildGolangRequestTrailers(req, trailers)) {
      // free memory
      cost_time_mem_ += measure<>::execution([&]() { freeReqAndResp(req, resp); });
      return Http::FilterTrailersStatus::Continue;
    }

    // call golang request stream filter
    cost_time_decode_ +=
        measure<>::execution([&]() { resp = dynamicLib_->runReceiveStreamFilter(req); });
    switch (doGolangResponseAndCleanup(req, resp, *request_headers_, true)) {
    case GolangStatus::Continue:
      return Http::FilterTrailersStatus::Continue;
    case GolangStatus::DirectResponse:
      return Http::FilterTrailersStatus::Continue;
    case GolangStatus::NeedAsync:
      return Http::FilterTrailersStatus::StopIteration;
    }
    */

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
    /*
    Request req{};
    Response resp{};
    // create request and response struct
    cost_time_mem_ += measure<>::execution(
        [&]() { initReqAndResp(req, resp, response_headers_->size(), trailers.size()); });

    // build golang request, if the build fails, then the filter should be
    // skipped
    if (!buildGolangRequestHeaders(req, *response_headers_)) {
      // free memory
      cost_time_mem_ += measure<>::execution([&]() { freeReqAndResp(req, resp); });
      return Http::FilterTrailersStatus::Continue;
    }

    // build golang request body
    Buffer::OwnedImpl empty;
    buildGolangRequestBodyEncode(req, empty);

    // build golang request trailers
    if (!buildGolangRequestTrailers(req, trailers)) {
      // free memory
      cost_time_mem_ += measure<>::execution([&]() { freeReqAndResp(req, resp); });
      return Http::FilterTrailersStatus::Continue;
    }

    // call golang request stream filter
    cost_time_encode_ +=
        measure<>::execution([&]() { resp = dynamicLib_->runSendStreamFilter(req); });
    switch (doGolangResponseAndCleanup(req, resp, *response_headers_, false)) {
    case GolangStatus::Continue:
      return Http::FilterTrailersStatus::Continue;
    case GolangStatus::DirectResponse:
      return Http::FilterTrailersStatus::Continue;
    case GolangStatus::NeedAsync:
      return Http::FilterTrailersStatus::StopIteration;
    }
    */

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang extension filter encodeTrailers catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang extension filter encodeTrailers catch unknown exception.");
  }

  return Http::FilterTrailersStatus::Continue;
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

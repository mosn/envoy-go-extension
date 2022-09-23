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

Event::Dispatcher* Filter::getDispatcher() { return &decoder_callbacks_->dispatcher(); }

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

void Filter::requestContinue() {
  // TODO: skip post event to dispatcher, and return continue in the caller,
  // when it's invoked in the current envoy thread, for better performance & latency.
  auto weak_ptr = weak_from_this();
  getDispatcher()->post([this, weak_ptr]{
    ASSERT(isThreadSafe());
    // do not need lock here, since it's the work thread now.
    if (!weak_ptr.expired() && !has_destroyed_) {
      ENVOY_LOG(debug, "golang filter callback continueDecoding");
      decoder_callbacks_->continueDecoding();
    } else {
      ENVOY_LOG(info, "golang filter has gone or destroyed in requestContinue event");
    }
  });
}

absl::optional<absl::string_view> Filter::getRequestHeader(absl::string_view key) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return "";
  }
  return request_headers_->getByKey(key);
}

void Filter::copyRequestHeaders(_GoString_ *goStrs, char *goBuf) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_destroyed_) {
    ENVOY_LOG(warn, "golang filter has been destroyed");
    return;
  }
  auto i = 0;
  request_headers_->iterate([this, &i, &goStrs, &goBuf](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
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
  getDispatcher()->post([this, weak_ptr]{
    ASSERT(isThreadSafe());
    // do not need lock here, since it's the work thread now.
    if (!weak_ptr.expired() && !has_destroyed_) {
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
  auto result = response_headers_->get(Http::LowerCaseString(key));

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
  response_headers_->iterate([this, &i, &goStrs, &goBuf](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
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
  response_headers_->setCopy(Http::LowerCaseString(key), value);
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

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  ENVOY_LOG(info, "golang filter decodeHeaders, end_stream: {}.", end_stream);

  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterHeadersStatus::Continue;
  }

  // save request headers
  request_headers_ = &headers;
  // headers.path();
  // headers.getByKey("foo");

  try {
    auto id = config_->getConfigId();
    FilterWeakPtrHolder* holder = new FilterWeakPtrHolder(weak_from_this());
    ptr_holder_ = reinterpret_cast<uint64_t>(holder);
    dynamicLib_->moeOnHttpDecodeHeader(ptr_holder_, id, end_stream, headers.size(), headers.byteSize());

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter decodeHeaders catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter decodeHeaders catch unknown exception.");
  }

  return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterDataStatus::Continue;
  }

  request_data_ = &data;

  try {
    ASSERT(ptr_holder_ != 0);
    auto id = config_->getConfigId();
    dynamicLib_->moeOnHttpDecodeData(ptr_holder_, id, reinterpret_cast<uint64_t>(&data), data.length(), end_stream);

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter decodeData catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter decodeData catch unknown exception.");
  }

  return Http::FilterDataStatus::StopIterationAndWatermark;
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

  response_headers_ = &headers;

  try {
    ASSERT(ptr_holder_ != 0);
    auto id = config_->getConfigId();
    dynamicLib_->moeOnHttpEncodeHeader(ptr_holder_, id, end_stream, headers.size(), headers.byteSize());

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter encodeHeaders catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter encodeHeaders catch unknown exception.");
  }

  // auto status = Http::FilterHeadersStatus::Continue;
  auto status = Http::FilterHeadersStatus::StopAllIterationAndWatermark;
  ENVOY_LOG(info, "golang filter encodeHeaders return with status: {}.", int(status));
  return status;
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (dynamicLib_ == nullptr) {
    ENVOY_LOG(error, "dynamicLib_ is nullPtr, maybe the instance already unpub.");
    // TODO return Network::FilterStatus::StopIteration and close connection immediately?
    return Http::FilterDataStatus::Continue;
  }

  try {
    ASSERT(ptr_holder_ != 0);
    auto id = config_->getConfigId();
    dynamicLib_->moeOnHttpEncodeData(ptr_holder_, id, reinterpret_cast<uint64_t>(&data), data.length(), end_stream);

  } catch (const EnvoyException& e) {
    ENVOY_LOG(error, "golang filter encodeData catch: {}.", e.what());

  } catch (...) {
    ENVOY_LOG(error, "golang filter encodeData catch unknown exception.");
  }
  auto status = Http::FilterDataStatus::StopIterationAndWatermark;
  ENVOY_LOG(info, "golang filter encodeData return with status: {}.", int(status));
  return status;
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

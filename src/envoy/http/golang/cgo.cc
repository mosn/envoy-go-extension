#include "src/envoy/http/golang/golang_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

//
// These functions may be invoked in another go thread,
// which means may introduce race between go thread and envoy thread.
//

extern "C" void moeHttpContinue(void* r, int status) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    filter->continueStatus(static_cast<GolangStatus>(status));
  }
}

absl::string_view copyGoString(void* str) {
  if (str == nullptr) {
    return "";
  }
  auto goStr = reinterpret_cast<GoString*>(str);
  return absl::string_view(goStr->p, goStr->n);
}

extern "C" void moeHttpSendLocalReply(void* r, int response_code, void* body_text, void* headers,
                                      long long int grpc_status, void* details) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    // TODO: headers
    auto grpcStatus = static_cast<Grpc::Status::GrpcStatus>(grpc_status);
    filter->sendLocalReply(static_cast<Http::Code>(response_code), copyGoString(body_text), nullptr,
                           grpcStatus, copyGoString(details));
  }
}

// unsafe API, without copy memory from c to go.
extern "C" void moeHttpGetHeader(void* r, void* key, void* value) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto keyStr = copyGoString(key);
    auto v = filter->getHeader(keyStr);
    if (v.has_value()) {
      auto goValue = reinterpret_cast<GoString*>(value);
      goValue->p = v.value().data();
      goValue->n = v.value().length();
    }
  }
}

extern "C" void moeHttpCopyHeaders(void* r, void* strs, void* buf) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    filter->copyHeaders(goStrs, goBuf);
  }
}

extern "C" void moeHttpSetHeader(void* r, void* key, void* value) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto keyStr = copyGoString(key);
    auto valueStr = copyGoString(value);
    filter->setHeader(keyStr, valueStr);
  }
}

extern "C" void moeHttpRemoveHeader(void* r, void* key) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    // TODO: it's safe to skip copy
    auto keyStr = copyGoString(key);
    filter->removeHeader(keyStr);
  }
}

extern "C" void moeHttpGetBuffer(void* r, unsigned long long int bufferPtr, void* data) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
  if (auto filter = weakFilter.lock()) {
    filter->copyBuffer(buffer, reinterpret_cast<char*>(data));
  }
}

extern "C" void moeHttpSetBuffer(void* r, unsigned long long int bufferPtr, void* data,
                                 int length) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
  if (auto filter = weakFilter.lock()) {
    auto value = absl::string_view(reinterpret_cast<const char*>(data), length);
    filter->setBuffer(buffer, value);
  }
}

extern "C" void moeHttpCopyTrailers(void* r, void* strs, void* buf) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    filter->copyTrailers(goStrs, goBuf);
  }
}

extern "C" void moeHttpSetTrailer(void* r, void* key, void* value) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto keyStr = copyGoString(key);
    auto valueStr = copyGoString(value);
    filter->setTrailer(keyStr, valueStr);
  }
}

extern "C" void moeHttpFinalize(void* r, int reason) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  delete req;
}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
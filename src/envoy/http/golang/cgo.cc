#include "src/envoy/http/golang/golang_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

//
// These functions may be invoked in another go thread,
// which means may introduce race between go thread and envoy thread.
//

absl::string_view copyGoString(void* str) {
  if (str == nullptr) {
    return "";
  }
  auto goStr = reinterpret_cast<GoString*>(str);
  return absl::string_view(goStr->p, goStr->n);
}

extern "C" {

void moeHandlerWrapper(void* r, std::function<void(std::shared_ptr<Filter>&)> f) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    f(filter);
  }
}

void moeHttpContinue(void* r, int status) {
  moeHandlerWrapper(r, [status](std::shared_ptr<Filter>& filter) {
    filter->continueStatus(static_cast<GolangStatus>(status));
  });
}

void moeHttpSendLocalReply(void* r, int response_code, void* body_text, void* headers,
                                      long long int grpc_status, void* details) {
  moeHandlerWrapper(r, [response_code, body_text, headers, grpc_status, details](std::shared_ptr<Filter>& filter) {
    (void)headers;
    auto grpcStatus = static_cast<Grpc::Status::GrpcStatus>(grpc_status);
    filter->sendLocalReply(static_cast<Http::Code>(response_code), copyGoString(body_text), nullptr,
                           grpcStatus, copyGoString(details));
  });
}

// unsafe API, without copy memory from c to go.
void moeHttpGetHeader(void* r, void* key, void* value) {
  moeHandlerWrapper(r, [key, value](std::shared_ptr<Filter>& filter) {
    auto keyStr = copyGoString(key);
    auto v = filter->getHeader(keyStr);
    if (v.has_value()) {
      auto goValue = reinterpret_cast<GoString*>(value);
      goValue->p = v.value().data();
      goValue->n = v.value().length();
    }
  });
}

void moeHttpCopyHeaders(void* r, void* strs, void* buf) {
  moeHandlerWrapper(r, [strs, buf](std::shared_ptr<Filter>& filter) {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    filter->copyHeaders(goStrs, goBuf);
  });
}

void moeHttpSetHeader(void* r, void* key, void* value) {
  moeHandlerWrapper(r, [key, value](std::shared_ptr<Filter>& filter) {
    auto keyStr = copyGoString(key);
    auto valueStr = copyGoString(value);
    filter->setHeader(keyStr, valueStr);
  });
}

void moeHttpRemoveHeader(void* r, void* key) {
  moeHandlerWrapper(r, [key](std::shared_ptr<Filter>& filter) {
    // TODO: it's safe to skip copy
    auto keyStr = copyGoString(key);
    filter->removeHeader(keyStr);
  });
}

void moeHttpGetBuffer(void* r, unsigned long long int bufferPtr, void* data) {
  moeHandlerWrapper(r, [bufferPtr, data](std::shared_ptr<Filter>& filter) {
    auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
    filter->copyBuffer(buffer, reinterpret_cast<char*>(data));
  });
}

void moeHttpSetBuffer(void* r, unsigned long long int bufferPtr, void* data,
                                 int length) {
  moeHandlerWrapper(r, [bufferPtr, data, length](std::shared_ptr<Filter>& filter) {
    auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
    auto value = absl::string_view(reinterpret_cast<const char*>(data), length);
    filter->setBuffer(buffer, value);
  });
}

void moeHttpCopyTrailers(void* r, void* strs, void* buf) {
  moeHandlerWrapper(r, [strs, buf](std::shared_ptr<Filter>& filter) {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    filter->copyTrailers(goStrs, goBuf);
  });
}

void moeHttpSetTrailer(void* r, void* key, void* value) {
  moeHandlerWrapper(r, [key, value](std::shared_ptr<Filter>& filter) {
    auto keyStr = copyGoString(key);
    auto valueStr = copyGoString(value);
    filter->setTrailer(keyStr, valueStr);
  });
}

void moeHttpFinalize(void* r, int reason) {
  (void)reason;
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  delete req;
}

}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

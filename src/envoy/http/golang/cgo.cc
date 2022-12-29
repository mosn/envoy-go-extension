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

absl::string_view stringViewFromGoSlice(void* slice) {
  if (slice == nullptr) {
    return "";
  }
  auto goSlice = reinterpret_cast<GoSlice*>(slice);
  return absl::string_view(static_cast<const char*>(goSlice->data), goSlice->len);
}

extern "C" {

int moeHandlerWrapper(void* r, std::function<int(std::shared_ptr<Filter>&)> f) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    return f(filter);
  }
  return CAPIFilterIsGone;
}

int moeHttpContinue(void* r, int status) {
  return moeHandlerWrapper(r, [status](std::shared_ptr<Filter>& filter) -> int {
    return filter->continueStatus(static_cast<GolangStatus>(status));
  });
}

int moeHttpSendLocalReply(void* r, int response_code, void* body_text, void* headers,
                          long long int grpc_status, void* details) {
  return moeHandlerWrapper(r,
                           [response_code, body_text, headers, grpc_status,
                            details](std::shared_ptr<Filter>& filter) -> int {
                             (void)headers;
                             auto grpcStatus = static_cast<Grpc::Status::GrpcStatus>(grpc_status);
                             return filter->sendLocalReply(static_cast<Http::Code>(response_code),
                                                           copyGoString(body_text), nullptr,
                                                           grpcStatus, copyGoString(details));
                           });
}

// unsafe API, without copy memory from c to go.
int moeHttpGetHeader(void* r, void* key, void* value) {
  return moeHandlerWrapper(r, [key, value](std::shared_ptr<Filter>& filter) -> int {
    auto keyStr = copyGoString(key);
    auto goValue = reinterpret_cast<GoString*>(value);
    return filter->getHeader(keyStr, goValue);
  });
}

int moeHttpCopyHeaders(void* r, void* strs, void* buf) {
  return moeHandlerWrapper(r, [strs, buf](std::shared_ptr<Filter>& filter) -> int {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    return filter->copyHeaders(goStrs, goBuf);
  });
}

int moeHttpSetHeaderHelper(void* r, void* key, void* value, headerAction act) {
  return moeHandlerWrapper(r, [key, value, act](std::shared_ptr<Filter>& filter) -> int {
    auto keyStr = copyGoString(key);
    auto valueStr = copyGoString(value);
    return filter->setHeader(keyStr, valueStr, act);
  });
}

int moeHttpRemoveHeader(void* r, void* key) {
  return moeHandlerWrapper(r, [key](std::shared_ptr<Filter>& filter) -> int {
    // TODO: it's safe to skip copy
    auto keyStr = copyGoString(key);
    return filter->removeHeader(keyStr);
  });
}

int moeHttpGetBuffer(void* r, unsigned long long int bufferPtr, void* data) {
  return moeHandlerWrapper(r, [bufferPtr, data](std::shared_ptr<Filter>& filter) -> int {
    auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
    return filter->copyBuffer(buffer, reinterpret_cast<char*>(data));
  });
}

int moeHttpSetBufferHelper(void* r, unsigned long long int bufferPtr, void* data, int length,
                           bufferAction action) {
  return moeHandlerWrapper(
      r, [bufferPtr, data, length, action](std::shared_ptr<Filter>& filter) -> int {
        auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
        auto value = absl::string_view(reinterpret_cast<const char*>(data), length);
        return filter->setBufferHelper(buffer, value, action);
      });
}

int moeHttpCopyTrailers(void* r, void* strs, void* buf) {
  return moeHandlerWrapper(r, [strs, buf](std::shared_ptr<Filter>& filter) -> int {
    auto goStrs = reinterpret_cast<GoString*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    return filter->copyTrailers(goStrs, goBuf);
  });
}

int moeHttpSetTrailer(void* r, void* key, void* value) {
  return moeHandlerWrapper(r, [key, value](std::shared_ptr<Filter>& filter) -> int {
    auto keyStr = copyGoString(key);
    auto valueStr = copyGoString(value);
    return filter->setTrailer(keyStr, valueStr);
  });
}

int moeHttpGetStringValue(void* r, int id, void* value) {
  return moeHandlerWrapper(r, [id, value](std::shared_ptr<Filter>& filter) -> int {
    auto valueStr = reinterpret_cast<GoString*>(value);
    return filter->getStringValue(id, valueStr);
  });
}

void moeHttpFinalize(void* r, int reason) {
  (void)reason;
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  delete req;
}

int moeHttpGetDynamicMetadata(void* r, void* name, void* buf) {
  return moeHandlerWrapper(r, [name, buf](std::shared_ptr<Filter>& filter) -> int {
    auto nameStr = std::string(copyGoString(name));
    auto bufSlice = reinterpret_cast<GoSlice*>(buf);
    return filter->getDynamicMetadata(nameStr, bufSlice);
  });
}

int moeHttpSetDynamicMetadata(void* r, void* name, void* key, void* buf) {
  return moeHandlerWrapper(r, [name, key, buf](std::shared_ptr<Filter>& filter) -> int {
    auto nameStr = std::string(copyGoString(name));
    auto keyStr = std::string(copyGoString(key));
    auto bufStr = stringViewFromGoSlice(buf);
    return filter->setDynamicMetadata(nameStr, keyStr, bufStr);
  });
}
}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

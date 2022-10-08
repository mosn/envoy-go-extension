#include "src/envoy/http/golang/golang_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

//
// These functions may be invoked in another go thread,
// which means may introduce race between go thread and envoy thread.
//

const int TlsHandshakerRespSync = 0;

extern "C" void moeHttpContinue(void *r, int status) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    filter->continueStatus(static_cast<GolangStatus>(status));
  }
}

// unsafe API, without copy memory from c to go.
extern "C" void moeHttpGetHeader(void *r, void *key, void *value) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto goKey = reinterpret_cast<_GoString_*>(key);
    auto keyStr = absl::string_view(goKey->p, goKey->n);
    auto v = filter->getHeader(keyStr);
    if (v.has_value()) {
      auto goValue = reinterpret_cast<_GoString_*>(value);
      goValue->p = v.value().data();
      goValue->n = v.value().length();
    }
  }
}

extern "C" void moeHttpCopyHeaders(void *r, void *strs, void *buf) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto goStrs = reinterpret_cast<_GoString_*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    filter->copyHeaders(goStrs, goBuf);
  }
}

extern "C" void moeHttpSetHeader(void *r, void *key, void *value) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  if (auto filter = weakFilter.lock()) {
    auto goKey = reinterpret_cast<_GoString_*>(key);
    auto goValue = reinterpret_cast<_GoString_*>(value);
    auto keyStr = absl::string_view(goKey->p, goKey->n);
    auto valueStr = absl::string_view(goValue->p, goValue->n);
    filter->setHeader(keyStr, valueStr);
  }
}

extern "C" void moeHttpGetBuffer(void *r, unsigned long long int bufferPtr, void *data) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
  if (auto filter = weakFilter.lock()) {
    filter->copyBuffer(buffer, reinterpret_cast<char*>(data));
  }
}

extern "C" void moeHttpSetBuffer(void *r, unsigned long long int bufferPtr, void *data, int length) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  auto weakFilter = req->weakFilter();
  auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
  if (auto filter = weakFilter.lock()) {
    auto value = absl::string_view(reinterpret_cast<const char*>(data), length);
    filter->setBuffer(buffer, value);
  }
}

extern "C" void moeHttpFinalize(void *r, int reason) {
  auto req = reinterpret_cast<httpRequestInternal*>(r);
  delete req;
}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
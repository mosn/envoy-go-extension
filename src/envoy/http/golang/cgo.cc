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

extern "C" void moeHttpDecodeContinue(unsigned long long int filterHolder, int end) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    filter->requestContinue();
  }
}

extern "C" void moeHttpGetRequestHeader(unsigned long long int filterHolder, void *key, void *value) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    auto goKey = reinterpret_cast<_GoString_*>(key);
    auto keyStr = absl::string_view(goKey->p, goKey->n);
    auto v = filter->getRequestHeader(keyStr);
    if (v.has_value()) {
      auto goValue = reinterpret_cast<_GoString_*>(value);
      goValue->p = v.value().data();
      goValue->n = v.value().length();
    }
  }
}

extern "C" void moeHttpCopyRequestHeaders(unsigned long long int filterHolder, void *strs, void *buf) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    auto goStrs = reinterpret_cast<_GoString_*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    filter->copyRequestHeaders(goStrs, goBuf);
  }
}

extern "C" void moeHttpEncodeContinue(unsigned long long int filterHolder, int end) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    filter->responseContinue();
  }

  // TODO: end = 1 in encodeData
  if (end == 1) {
    delete holder;
  }
}

extern "C" void moeHttpGetResponseHeader(unsigned long long int filterHolder, void *key, void *value) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    auto goKey = reinterpret_cast<_GoString_*>(key);
    auto keyStr = absl::string_view(goKey->p, goKey->n);
    auto v = filter->getResponseHeader(keyStr);
    if (v.has_value()) {
      auto goValue = reinterpret_cast<_GoString_*>(value);
      goValue->p = v.value().data();
      goValue->n = v.value().length();
    }
  }
}

extern "C" void moeHttpCopyResponseHeaders(unsigned long long int filterHolder, void *strs, void *buf) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    auto goStrs = reinterpret_cast<_GoString_*>(strs);
    auto goBuf = reinterpret_cast<char*>(buf);
    filter->copyResponseHeaders(goStrs, goBuf);
  }
}

extern "C" void moeHttpSetResponseHeader(unsigned long long int filterHolder, void *key, void *value) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    auto goKey = reinterpret_cast<_GoString_*>(key);
    auto goValue = reinterpret_cast<_GoString_*>(value);
    auto keyStr = absl::string_view(goKey->p, goKey->n);
    auto valueStr = absl::string_view(goValue->p, goValue->n);
    filter->setResponseHeader(keyStr, valueStr);
  }
}

extern "C" void moeHttpGetBuffer(unsigned long long int filterHolder, unsigned long long int bufferPtr, void *data) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto buffer = reinterpret_cast<Buffer::Instance*>(bufferPtr);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    filter->copyBuffer(buffer, reinterpret_cast<char*>(data));
  }
}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
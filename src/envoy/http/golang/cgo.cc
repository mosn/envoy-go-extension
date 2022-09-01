#include "src/envoy/http/golang/golang_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

//
// DownStream Tls Handshaker
//

const int TlsHandshakerRespSync = 0;

extern "C" void moeHttpRequestContinue(unsigned long long int filterHolder, int end) {
  if (filterHolder == 0) {
    return;
  }

  auto holder = reinterpret_cast<FilterWeakPtrHolder*>(filterHolder);
  auto weakFilter = holder->get();
  if (auto filter = weakFilter.lock()) {
    filter->requestContinue();
  }

  if (end == 1) {
    delete holder;
  }
}

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
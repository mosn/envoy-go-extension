#include "src/common/dso/dso.h"

namespace Envoy {
namespace Dso {

std::map<std::string, DsoInstance*> DsoInstanceManager::dso_map_ = {};
std::shared_mutex DsoInstanceManager::mutex_ = {};

bool DsoInstanceManager::pub(std::string dsoId, std::string dsoName) {
  if (getDsoInstanceByID(dsoId) != NULL) {
    ENVOY_LOG_MISC(error, "pub {} {} dso instance failed: already pub.", dsoId, dsoName);
    return false;
  }

  std::unique_lock<std::shared_mutex> w_lock(DsoInstanceManager::mutex_);

  DsoInstance* dso = new DsoInstance(dsoName);
  dso_map_[dsoId] = dso;
  return true;
}

bool DsoInstanceManager::unpub(std::string dsoId) {
  // TODO need delete dso
  std::unique_lock<std::shared_mutex> w_lock(DsoInstanceManager::mutex_);
  ENVOY_LOG_MISC(warn, "unpub {} dso instance.", dsoId);
  return dso_map_.erase(dsoId) == 1;
}

DsoInstance* DsoInstanceManager::getDsoInstanceByID(std::string dsoId) {
  std::shared_lock<std::shared_mutex> r_lock(DsoInstanceManager::mutex_);
  auto it = dso_map_.find(dsoId);
  if (it != dso_map_.end()) {
    return it->second;
  }

  return NULL;
}

// TODO implement
std::string DsoInstanceManager::show() {
  std::shared_lock<std::shared_mutex> r_lock(DsoInstanceManager::mutex_);
  std::string ids = "";
  for (auto it = dso_map_.begin(); it != dso_map_.end(); ++it) {
    ids += it->first;
    ids += ",";
  }
  return ids;
}

DsoInstance::DsoInstance(const std::string dsoName) : dsoName_(dsoName) {

  handler_ = dlopen(dsoName.c_str(), RTLD_LAZY);
  if (!handler_) {
    ENVOY_LOG_MISC(error, "cannot load : {} error: {}", dsoName, dlerror());
    return;
  }

  void* func;

  func = dlsym(handler_, "setPostDecodeCallbackFunc");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: setPostDecodeCallbackFunc, err: {}",
                   dsoName_, dlerror());
    return;
  }
  setPostDecodeCallbackFunc_ = reinterpret_cast<void (*)(fc)>(func);

  func = dlsym(handler_, "setPostEncodeCallbackFunc");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: setPostEncodeCallbackFunc, err: {}",
                   dsoName_, dlerror());
    return;
  }
  setPostEncodeCallbackFunc_ = reinterpret_cast<void (*)(fc)>(func);

  func = dlsym(handler_, "runReceiveStreamFilter");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: runReceiveStreamFilter, err: {}", dsoName_,
                   dlerror());
    return;
  }
  runReceiveStreamFilter_ = reinterpret_cast<Response (*)(Request)>(func);

  func = dlsym(handler_, "runSendStreamFilter");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: runSendStreamFilter, err: {}", dsoName_,
                   dlerror());
    return;
  }
  runSendStreamFilter_ = reinterpret_cast<Response (*)(Request)>(func);

  func = dlsym(handler_, "destoryStream");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: destoryStream, err: {}", dsoName_,
                   dlerror());
    return;
  }
  destoryStream_ = reinterpret_cast<void (*)(long long unsigned int, int)>(func);

  func = dlsym(handler_, "invokeClusterWarmingCb");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: invokeClusterWarmingCb, err: {}", dsoName_,
                   dlerror());
    return;
  }
  invokeClusterWarmingCb_ =
      reinterpret_cast<void (*)(char*, long long unsigned int, long long unsigned int, int)>(func);

  func = dlsym(handler_, "moeOnNewDownstreamConnection");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnNewDownstreamConnection, err: {}",
                   dsoName_, dlerror());
    return;
  }
  moeOnNewDownstreamConnection_ =
      reinterpret_cast<GoUint64 (*)(GoInt64, GoUint64, GoUint64, GoUint64, GoUint64, GoUint64,
                                    GoUint64, GoUint64, GoUint64)>(func);

  func = dlsym(handler_, "moeOnDownstreamEvent");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnDownstreamEvent, err: {}", dsoName_,
                   dlerror());
    return;
  }
  moeOnDownstreamEvent_ = reinterpret_cast<void (*)(GoInt64, GoUint64, GoInt)>(func);

  func = dlsym(handler_, "moeOnDownstreamData");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnDownstreamData, err: {}", dsoName_,
                   dlerror());
    return;
  }
  moeOnDownstreamData_ =
      reinterpret_cast<void (*)(GoInt64, GoUint64, GoUint64, GoUint64, GoInt, GoInt)>(func);

  func = dlsym(handler_, "moeOnUpstreamConnPoolReady");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnUpstreamConnPoolReady, err: {}",
                   dsoName_, dlerror());
    return;
  }
  moeOnUpstreamConnPoolReady_ = reinterpret_cast<void (*)(GoInt64, GoUint64, GoUint64, GoUint64,
                                                          GoUint64, GoUint64, GoUint64)>(func);

  func = dlsym(handler_, "moeOnUpstreamConnPoolFailure");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnUpstreamConnPoolFailure, err: {}",
                   dsoName_, dlerror());
    return;
  }
  moeOnUpstreamConnPoolFailure_ = reinterpret_cast<void (*)(GoInt64, GoUint64, GoInt)>(func);

  func = dlsym(handler_, "moeOnUpstreamEvent");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnUpstreamEvent, err: {}", dsoName_,
                   dlerror());
    return;
  }
  moeOnUpstreamEvent_ = reinterpret_cast<void (*)(GoInt64, GoUint64, GoInt)>(func);

  func = dlsym(handler_, "moeOnUpstreamData");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnUpstreamData, err: {}", dsoName_,
                   dlerror());
    return;
  }
  moeOnUpstreamData_ =
      reinterpret_cast<void (*)(GoInt64, GoUint64, GoUint64, GoUint64, GoInt, GoInt)>(func);

  func = dlsym(handler_, "moeOnDownstreamTlsInfo");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnDownstreamTlsInfo, err: {}", dsoName_,
                   dlerror());
    return;
  }
  moeOnDownstreamTlsInfo_ = reinterpret_cast<void (*)(GoInt64, GoUint64, TlsInfo*)>(func);

  func = dlsym(handler_, "moeOnUpstreamTlsInfo");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnUpstreamTlsInfo, err: {}", dsoName_,
                   dlerror());
    return;
  }
  moeOnUpstreamTlsInfo_ = reinterpret_cast<void (*)(GoInt64, GoUint64, TlsInfo*)>(func);

  func = dlsym(handler_, "moeOnTlsHandshakerSelectCert");
  if (!func) {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnTlsHandshakerSelectCert, err: {}", dsoName_,
                   dlerror());
    return;
  }
  moeOnTlsHandshakerSelectCert_ = reinterpret_cast<void (*)(GoUint64 p0, GoUint64 p1, GoUint64 p2)>(func);
}

DsoInstance::~DsoInstance() {

  setPostDecodeCallbackFunc_ = nullptr;
  setPostEncodeCallbackFunc_ = nullptr;
  runReceiveStreamFilter_ = nullptr;
  runSendStreamFilter_ = nullptr;
  destoryStream_ = nullptr;
  invokeClusterWarmingCb_ = nullptr;
  moeOnNewDownstreamConnection_ = nullptr;
  moeOnDownstreamEvent_ = nullptr;
  moeOnDownstreamData_ = nullptr;
  moeOnUpstreamConnPoolReady_ = nullptr;
  moeOnUpstreamConnPoolFailure_ = nullptr;
  moeOnUpstreamEvent_ = nullptr;
  moeOnUpstreamData_ = nullptr;
  moeOnDownstreamTlsInfo_ = nullptr;
  moeOnUpstreamTlsInfo_ = nullptr;
  moeOnTlsHandshakerSelectCert_ = nullptr;

  dlclose(handler_);
  handler_ = nullptr;
}

void DsoInstance::setPostDecodeCallbackFunc(fc p0) {
  if (setPostDecodeCallbackFunc_) {
    setPostDecodeCallbackFunc_(p0);
  }
}

void DsoInstance::setPostEncodeCallbackFunc(fc p0) {
  if (setPostEncodeCallbackFunc_) {
    setPostEncodeCallbackFunc_(p0);
  }
}

Response DsoInstance::runReceiveStreamFilter(Request p0) {
  if (runReceiveStreamFilter_) {
    return runReceiveStreamFilter_(p0);
  }
  return Response{};
}

Response DsoInstance::runSendStreamFilter(Request p0) {
  if (runSendStreamFilter_) {
    return runSendStreamFilter_(p0);
  }
  return Response{};
}

void DsoInstance::destoryStream(long long unsigned int p0, int p1) {
  if (destoryStream_) {
    destoryStream_(p0, p1);
  }
}

void DsoInstance::invokeClusterWarmingCb(char* p0, long long unsigned int p1,
                                         long long unsigned int p2, int p3) {
  if (invokeClusterWarmingCb_) {
    invokeClusterWarmingCb_(p0, p1, p2, p3);
  }
}

GoUint64 DsoInstance::moeOnNewDownstreamConnection(GoInt64 p0, GoUint64 p1, GoUint64 p2,
                                                   GoUint64 p3, GoUint64 p4, GoUint64 p5,
                                                   GoUint64 p6, GoUint64 p7, GoUint64 p8) {
  if (moeOnNewDownstreamConnection_) {
    return moeOnNewDownstreamConnection_(p0, p1, p2, p3, p4, p5, p6, p7, p8);
  }
  return 0;
}

void DsoInstance::moeOnDownstreamEvent(GoInt64 p0, GoUint64 p1, GoInt p2) {
  if (moeOnDownstreamEvent_) {
    moeOnDownstreamEvent_(p0, p1, p2);
  }
}

void DsoInstance::moeOnDownstreamData(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4,
                                      GoInt p5) {
  if (moeOnDownstreamData_) {
    moeOnDownstreamData_(p0, p1, p2, p3, p4, p5);
  }
}

void DsoInstance::moeOnUpstreamConnPoolReady(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3,
                                             GoUint64 p4, GoUint64 p5, GoUint64 p6) {
  if (moeOnUpstreamConnPoolReady_) {
    moeOnUpstreamConnPoolReady_(p0, p1, p2, p3, p4, p5, p6);
  }
}

void DsoInstance::moeOnUpstreamConnPoolFailure(GoInt64 p0, GoUint64 p1, GoInt p2) {
  if (moeOnUpstreamConnPoolFailure_) {
    moeOnUpstreamConnPoolFailure_(p0, p1, p2);
  }
}

void DsoInstance::moeOnUpstreamEvent(GoInt64 p0, GoUint64 p1, GoInt p2) {
  if (moeOnUpstreamEvent_) {
    moeOnUpstreamEvent_(p0, p1, p2);
  }
}

void DsoInstance::moeOnUpstreamData(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4,
                                    GoInt p5) {
  if (moeOnUpstreamData_) {
    moeOnUpstreamData_(p0, p1, p2, p3, p4, p5);
  }
}

void DsoInstance::moeOnDownstreamTlsInfo(GoInt64 p0, GoUint64 p1, TlsInfo* p2) {
  if (moeOnDownstreamTlsInfo_) {
    moeOnDownstreamTlsInfo_(p0, p1, p2);
  }
}

void DsoInstance::moeOnUpstreamTlsInfo(GoInt64 p0, GoUint64 p1, TlsInfo* p2) {
  if (moeOnUpstreamTlsInfo_) {
    moeOnUpstreamTlsInfo_(p0, p1, p2);
  }
}

void DsoInstance::moeOnTlsHandshakerSelectCert(GoUint64 p0, GoUint64 p1, GoUint64 p2) {
  if (moeOnTlsHandshakerSelectCert_) {
    moeOnTlsHandshakerSelectCert_(p0, p1, p2);
  }
}

} // namespace Dso
} // namespace Envoy

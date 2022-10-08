#include "src/envoy/common/dso/dso.h"

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
  ENVOY_LOG_MISC(info, "loading symbols from so file: {}", dsoName);

  handler_ = dlopen(dsoName.c_str(), RTLD_LAZY);
  if (!handler_) {
    ENVOY_LOG_MISC(error, "cannot load : {} error: {}", dsoName, dlerror());
    return;
  }

  auto func = dlsym(handler_, "moeNewHttpPluginConfig");
  if (func) {
    moeNewHttpPluginConfig_ = reinterpret_cast<GoUint64 (*)(GoUint64 p0, GoUint64 p1)>(func);
  } else {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeNewHttpPluginConfig, err: {}", dsoName,
                   dlerror());
  }

  func = dlsym(handler_, "moeOnHttpHeader");
  if (func) {
    moeOnHttpHeader_ = reinterpret_cast<GoUint64 (*)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5)>(func);
  } else {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnHttpHeader, err: {}", dsoName,
                   dlerror());
  }

  func = dlsym(handler_, "moeOnHttpData");
  if (func) {
    moeOnHttpData_ = reinterpret_cast<GoUint64 (*)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5)>(func);
  } else {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnHttpDecodeData, err: {}", dsoName,
                   dlerror());
  }

  func = dlsym(handler_, "moeOnHttpDestroy");
  if (func) {
    moeOnHttpDestroy_ = reinterpret_cast<void (*)(GoUint64 p0, GoUint64 p1)>(func);
  } else {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnHttpDecodeDestroy, err: {}", dsoName,
                   dlerror());
  }
}

DsoInstance::~DsoInstance() {
  moeNewHttpPluginConfig_ = nullptr;
  moeOnHttpHeader_ = nullptr;
  moeOnHttpData_ = nullptr;
  moeOnHttpDestroy_ = nullptr;

  dlclose(handler_);
  handler_ = nullptr;
}

GoUint64 DsoInstance::moeNewHttpPluginConfig(GoUint64 p0, GoUint64 p1) {
  if (moeNewHttpPluginConfig_) {
    return moeNewHttpPluginConfig_(p0, p1);
  }
  return 0;
}

GoUint64 DsoInstance::moeOnHttpHeader(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5) {
  if (moeOnHttpHeader_) {
    return moeOnHttpHeader_(p0, p1, p2, p3, p4, p5);
  }
  return 0;
}

GoUint64 DsoInstance::moeOnHttpData(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5) {
  if (moeOnHttpData_) {
    return moeOnHttpData_(p0, p1, p2, p3, p4, p5);
  }
  return 0;
}

void DsoInstance::moeOnHttpDestroy(GoUint64 p0, int p1) {
  if (moeOnHttpDestroy_) {
    moeOnHttpDestroy_(p0, GoUint64(p1));
  }
}

} // namespace Dso
} // namespace Envoy

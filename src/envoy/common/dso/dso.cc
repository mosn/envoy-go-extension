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

  auto func = dlsym(handler_, "moeOnHttpRequestHeader");
  if (func) {
    moeOnHttpRequestHeader_= reinterpret_cast<void (*)(GoUint64 p0, GoUint64 p1)>(func);
  } else {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnHttpRequestHeader, err: {}", dsoName,
                   dlerror());
  }

  func = dlsym(handler_, "moeOnHttpRequestData");
  if (func) {
    moeOnHttpRequestData_= reinterpret_cast<void (*)(GoUint64 p0, GoUint64 p1)>(func);
  } else {
    ENVOY_LOG_MISC(error, "lib: {}, cannot find symbol: moeOnHttpRequestData, err: {}", dsoName,
                   dlerror());
  }
}

DsoInstance::~DsoInstance() {
  moeOnHttpRequestHeader_ = nullptr;
  moeOnHttpRequestData_ = nullptr;

  dlclose(handler_);
  handler_ = nullptr;
}

void DsoInstance::moeOnHttpRequestHeader(GoUint64 p0, GoUint64 p1) {
  if (moeOnHttpRequestHeader_) {
    moeOnHttpRequestHeader_(p0, p1);
  }
}

void DsoInstance::moeOnHttpRequestData(GoUint64 p0, GoUint64 p1) {
  if (moeOnHttpRequestData_) {
    moeOnHttpRequestData_(p0, p1);
  }
}

} // namespace Dso
} // namespace Envoy

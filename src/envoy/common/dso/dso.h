#pragma once

#include <string>
#include <dlfcn.h>
#include <shared_mutex>

#include "source/common/common/logger.h"

#include "src/envoy/common/dso/libgolang.h"

namespace Envoy {
namespace Dso {

class DsoInstance {
public:
  DsoInstance(const std::string dsoName);
  ~DsoInstance();

  void moeOnHttpRequestHeader(GoUint64 p0, GoUint64 p1);
  void moeOnHttpRequestData(GoUint64 p0, GoUint64 p1);

private:
  const std::string dsoName_;
  void* handler_;

  void (*moeOnHttpRequestHeader_)(GoUint64 p0, GoUint64 p1) = {nullptr};
  void (*moeOnHttpRequestData_)(GoUint64 p0, GoUint64 p1) = {nullptr};
};

class DsoInstanceManager {
public:
  static bool pub(std::string dsoId, std::string dsoName);
  static bool unpub(std::string dsoId);
  static DsoInstance* getDsoInstanceByID(std::string dsoId);
  static std::string show();

private:
  static std::shared_mutex mutex_;
  static std::map<std::string, DsoInstance*> dso_map_;
};

} // namespace Dso
} // namespace Envoy

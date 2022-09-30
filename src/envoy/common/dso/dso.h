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

  GoUint64 moeNewHttpPluginConfig(GoUint64 p0, GoUint64 p1);

  GoUint64 moeOnHttpHeader(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5);
  GoUint64 moeOnHttpData(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5);

private:
  const std::string dsoName_;
  void* handler_;

  GoUint64 (*moeNewHttpPluginConfig_)(GoUint64 p0, GoUint64 p1) = {nullptr};

  GoUint64 (*moeOnHttpHeader_)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5) = {nullptr};
  GoUint64 (*moeOnHttpData_)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4, GoUint64 p5) = {nullptr};
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

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
  GoUint64 moeOnHttpDecodeHeader(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4);
  GoUint64 moeOnHttpDecodeData(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4);
  void moeOnHttpEncodeHeader(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4);
  void moeOnHttpEncodeData(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4);

private:
  const std::string dsoName_;
  void* handler_;

  GoUint64 (*moeNewHttpPluginConfig_)(GoUint64 p0, GoUint64 p1) = {nullptr};
  GoUint64 (*moeOnHttpDecodeHeader_)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4) = {nullptr};
  GoUint64 (*moeOnHttpDecodeData_)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4) = {nullptr};
  void (*moeOnHttpEncodeHeader_)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4) = {nullptr};
  void (*moeOnHttpEncodeData_)(GoUint64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4) = {nullptr};
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

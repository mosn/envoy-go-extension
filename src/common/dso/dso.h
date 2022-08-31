#pragma once

#include <string>
#include <dlfcn.h>
#include <shared_mutex>

#include "source/common/common/logger.h"

#include "src/common/dso/libgolang_extension.h"

namespace Envoy {
namespace Dso {

class DsoInstance {
public:
  DsoInstance(const std::string dsoName);
  ~DsoInstance();

  void setPostDecodeCallbackFunc(fc p0);
  void setPostEncodeCallbackFunc(fc p0);
  Response runReceiveStreamFilter(Request p0);
  Response runSendStreamFilter(Request p0);
  void destoryStream(long long unsigned int p0, int p1);
  void invokeClusterWarmingCb(char* p0, long long unsigned int p1, long long unsigned int p2,
                              int p3);
  GoUint64 moeOnNewDownstreamConnection(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3,
                                        GoUint64 p4, GoUint64 p5, GoUint64 p6, GoUint64 p7,
                                        GoUint64 p8);
  void moeOnDownstreamEvent(GoInt64 p0, GoUint64 p1, GoInt p2);
  void moeOnDownstreamData(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4, GoInt p5);
  void moeOnUpstreamConnPoolReady(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoUint64 p4,
                                  GoUint64 p5, GoUint64 p6);
  void moeOnUpstreamConnPoolFailure(GoInt64 p0, GoUint64 p1, GoInt p2);
  void moeOnUpstreamEvent(GoInt64 p0, GoUint64 p1, GoInt p2);
  void moeOnUpstreamData(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4, GoInt p5);
  void moeOnDownstreamTlsInfo(GoInt64 p0, GoUint64 p1, TlsInfo* p2);
  void moeOnUpstreamTlsInfo(GoInt64 p0, GoUint64 p1, TlsInfo* p2);
  void moeOnTlsHandshakerSelectCert(GoUint64 p0, GoUint64 p1, GoUint64 p2);

private:
  const std::string dsoName_;
  void* handler_;

  void (*setPostDecodeCallbackFunc_)(fc p0) = {nullptr};
  void (*setPostEncodeCallbackFunc_)(fc p0) = {nullptr};
  Response (*runReceiveStreamFilter_)(Request p0) = {nullptr};
  Response (*runSendStreamFilter_)(Request p0) = {nullptr};
  void (*destoryStream_)(long long unsigned int p0, int p1) = {nullptr};
  void (*invokeClusterWarmingCb_)(char* p0, long long unsigned int p1, long long unsigned int p2,
                                  int p3) = {nullptr};
  GoUint64 (*moeOnNewDownstreamConnection_)(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3,
                                            GoUint64 p4, GoUint64 p5, GoUint64 p6, GoUint64 p7,
                                            GoUint64 p8) = {nullptr};
  void (*moeOnDownstreamEvent_)(GoInt64 p0, GoUint64 p1, GoInt p2) = {nullptr};
  void (*moeOnDownstreamData_)(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4,
                               GoInt p5) = {nullptr};
  void (*moeOnUpstreamConnPoolReady_)(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3,
                                      GoUint64 p4, GoUint64 p5, GoUint64 p6) = {nullptr};
  void (*moeOnUpstreamConnPoolFailure_)(GoInt64 p0, GoUint64 p1, GoInt p2) = {nullptr};
  void (*moeOnUpstreamEvent_)(GoInt64 p0, GoUint64 p1, GoInt p2) = {nullptr};
  void (*moeOnUpstreamData_)(GoInt64 p0, GoUint64 p1, GoUint64 p2, GoUint64 p3, GoInt p4,
                             GoInt p5) = {nullptr};
  void (*moeOnDownstreamTlsInfo_)(GoInt64 p0, GoUint64 p1, TlsInfo* p2) = {nullptr};
  void (*moeOnUpstreamTlsInfo_)(GoInt64 p0, GoUint64 p1, TlsInfo* p2) = {nullptr};
  void (*moeOnTlsHandshakerSelectCert_)(GoUint64 p0, GoUint64 p1, GoUint64 p2) = {nullptr};
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

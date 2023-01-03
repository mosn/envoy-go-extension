#pragma once

#include "api/http/cluster/v3/cluster.pb.h"
#include "api/http/cluster/v3/cluster.pb.validate.h"

#include "source/common/common/base64.h"
#include "source/common/http/utility.h"

#include "src/envoy/common/dso/dso.h"

namespace Envoy {
namespace Router {
namespace Golang {

using GolangClusterProto = envoy::extensions::clusters::golang::v3::Config;

class ClusterConfig : Logger::Loggable<Logger::Id::http> {
public:
  ClusterConfig(const GolangClusterProto& config);
  uint64_t getConfigId();
  const std::string& defaultCluster() { return default_cluster_; }

private:
  const std::string so_id_;
  const std::string default_cluster_;
  const Protobuf::Any config_;
  uint64_t config_id_{0};
};

using ClusterConfigSharedPtr = std::shared_ptr<ClusterConfig>;

class GolangClusterSpecifierPlugin : public ClusterSpecifierPlugin,
                                     Logger::Loggable<Logger::Id::http> {
public:
  GolangClusterSpecifierPlugin(ClusterConfigSharedPtr config, Dso::DsoInstance* dynamicLib)
      : config_(config), dynamicLib_(dynamicLib){};

  RouteConstSharedPtr route(const RouteEntry& parent, const Http::RequestHeaderMap&) const override;

private:
  ClusterConfigSharedPtr config_;
  Dso::DsoInstance* dynamicLib_;
};

} // namespace Golang
} // namespace Router
} // namespace Envoy

#pragma once

#include "source/extensions/filters/http/common/factory_base.h"

#include "src/envoy/http/cluster/golang_cluster.h"

namespace Envoy {
namespace Router {
namespace Golang {

class GolangClusterSpecifierPluginFactoryConfig : public ClusterSpecifierPluginFactoryConfig {
public:
  GolangClusterSpecifierPluginFactoryConfig() = default;
  ClusterSpecifierPluginSharedPtr
  createClusterSpecifierPlugin(const Protobuf::Message& config,
                               Server::Configuration::CommonFactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<GolangClusterProto>();
  }

  std::string name() const override { return "envoy.router.cluster_specifier_plugin.golang"; }
};

} // namespace Golang
} // namespace Router
} // namespace Envoy

#pragma once

#include "envoy/common/pure.h"
#include "api/dso/v3/dso.pb.h"
#include "api/dso/v3/dso.pb.validate.h"
#include "envoy/server/bootstrap_extension_config.h"
#include "envoy/server/filter_config.h"
#include "envoy/server/instance.h"
#include "src/envoy/common/dso/dso.h"

#include "source/common/protobuf/protobuf.h"

namespace Envoy {
namespace Extensions {
namespace Bootstrap {
namespace Dso {

class DsoFactory : public Server::Configuration::BootstrapExtensionFactory {
public:
  ~DsoFactory() override = default;
  std::string name() const override { return "envoy.bootstrap.dso"; }
  Server::BootstrapExtensionPtr
  createBootstrapExtension(const Protobuf::Message& config,
                           Server::Configuration::ServerFactoryContext& context) override;
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<envoy::extensions::dso::v3::dso>();
  }
};

// TODO using dso log instead of golang
class DsoExtension : public Server::BootstrapExtension, Logger::Loggable<Logger::Id::misc> {
public:
  DsoExtension(const envoy::extensions::dso::v3::dso& config,
               Server::Configuration::ServerFactoryContext& context)
      : config_(config), context_(context) {}
  void onServerInitialized() override;

private:
  Server::ServerLifecycleNotifier::HandlePtr handler_{};
  envoy::extensions::dso::v3::dso config_;
  Server::Configuration::ServerFactoryContext& context_;
};

} // namespace Dso
} // namespace Bootstrap
} // namespace Extensions
} // namespace Envoy

#include "src/envoy/bootstrap/dso/config.h"

#include "envoy/registry/registry.h"
#include "envoy/server/factory_context.h"

#include "source/common/common/empty_string.h"
#include "source/common/config/datasource.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Extensions {
namespace Bootstrap {
namespace Dso {

void DsoExtension::onServerInitialized() {
  handler_ = context_.lifecycleNotifier().registerCallback(
      Server::ServerLifecycleNotifier::Stage::PostInit,
      [so_id = config_.so_id(), so_path = config_.so_path()] {
        Envoy::Dso::DsoInstanceManager::pub(so_id, so_path);
        ENVOY_LOG(warn, "DsoExtension::postworker: {} {}", so_id, so_path);
      });
}

Server::BootstrapExtensionPtr
DsoFactory::createBootstrapExtension(const Protobuf::Message& config,
                                     Server::Configuration::ServerFactoryContext& context) {
  auto typed_config = MessageUtil::downcastAndValidate<const envoy::extensions::dso::v3::dso&>(
      config, context.messageValidationContext().staticValidationVisitor());

  return std::make_unique<DsoExtension>(typed_config, context);
}

// /**
//  * Static registration for the dso factory. @see RegistryFactory.
//  */
REGISTER_FACTORY(DsoFactory, Server::Configuration::BootstrapExtensionFactory);

} // namespace Dso
} // namespace Bootstrap
} // namespace Extensions
} // namespace Envoy

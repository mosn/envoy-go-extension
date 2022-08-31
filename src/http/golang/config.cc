#include "src/http/golang/config.h"

#include "envoy/registry/registry.h"

#include "src/http/golang/golang_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

Http::FilterFactoryCb GolangFilterConfig::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::golang::v3::Config& proto_config, const std::string&,
    Server::Configuration::FactoryContext& factory_context) {

  FilterConfigSharedPtr config =
      std::make_shared<FilterConfig>(proto_config);

  return [&factory_context, config](Http::FilterChainFactoryCallbacks& callbacks) {
    auto filter =
        std::make_shared<Filter>(factory_context.grpcContext(), config, Filter::global_stream_id_++,
                                 Dso::DsoInstanceManager::getDsoInstanceByID(config->so_id()));
    callbacks.addStreamFilter(filter);
    callbacks.addAccessLogHandler(filter);
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
GolangFilterConfig::createRouteSpecificFilterConfigTyped(
    const envoy::extensions::filters::http::golang::v3::ConfigsPerRoute& proto_config,
    Server::Configuration::ServerFactoryContext& context, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<FilterConfigPerRoute>(proto_config, context);
}

/**
 * Static registration for the golang extensions filter. @see RegisterFactory.
 */
REGISTER_FACTORY(GolangFilterConfig,
                 Server::Configuration::NamedHttpFilterConfigFactory){"envoy.golang"};

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

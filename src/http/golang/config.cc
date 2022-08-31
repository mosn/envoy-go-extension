#include "src/http/golang/config.h"

#include "envoy/registry/registry.h"

#include "src/http/golang/golang_extension_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace GolangExtention {

Http::FilterFactoryCb GolangExtentionFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::golang::v3::Config& proto_config, const std::string&,
    Server::Configuration::FactoryContext& factory_context) {

  GolangExtentionFilterConfigSharedPtr config =
      std::make_shared<GolangExtentionFilterConfig>(proto_config);

  return [&factory_context, config](Http::FilterChainFactoryCallbacks& callbacks) {
    auto filter =
        std::make_shared<Filter>(factory_context.grpcContext(), config, Filter::global_stream_id_++,
                                 Dso::DsoInstanceManager::getDsoInstanceByID(config->so_id()));
    callbacks.addStreamFilter(filter);
    callbacks.addAccessLogHandler(filter);
  };
}

/**
 * Static registration for the golang extensions filter. @see RegisterFactory.
 */
REGISTER_FACTORY(GolangExtentionFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace GolangExtention
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

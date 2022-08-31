#pragma once

#include "api/http/golang/v3/golang.pb.h"
#include "api/http/golang/v3/golang.pb.validate.h"

#include "source/extensions/filters/http/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace GolangExtention {

constexpr char CanonicalName[] = "envoy.filters.http.golang";

/**
 * Config registration for the golang extentions  filter. @see
 * NamedHttpFilterConfigFactory.
 */
class GolangExtentionFilterConfigFactory
    : public Common::FactoryBase<envoy::extensions::filters::http::golang::v3::Config> {
public:
  GolangExtentionFilterConfigFactory() : FactoryBase(CanonicalName) {}

  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::golang::v3::Config& proto_config,
      const std::string& stats_prefix,
      Server::Configuration::FactoryContext& factory_context) override;
};

} // namespace GolangExtention
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

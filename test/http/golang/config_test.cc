#include <string>

#include "src/envoy/http/golang/config.h"

#include "envoy/registry/registry.h"

#include "src/envoy/http/golang/golang_filter.h"

#include "test/mocks/server/factory_context.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {
namespace {

TEST(GolangFilterConfigTest, InvalidateEmptyConfig) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  EXPECT_THROW_WITH_REGEX(
      GolangFilterConfig().createFilterFactoryFromProto(
          envoy::extensions::filters::http::golang::v3::Config(), "stats", context),
      Envoy::ProtoValidationException,
      "ConfigValidationError.SoId: value length must be at least 1 bytes");
}

TEST(GolangFilterConfigTest, GolangFilterWithValidConfig) {
  const std::string yaml_string = R"EOF(
  so_id: mosn
  plugin_name: xx
  merge_policy: MERGE_VIRTUALHOST_ROUTER_FILTER
  plugin_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: typexx
    value:
        key: value
        int: 10
  )EOF";

  envoy::extensions::filters::http::golang::v3::Config proto_config;
  TestUtility::loadFromYaml(yaml_string, proto_config);
  NiceMock<Server::Configuration::MockFactoryContext> context;
  GolangFilterConfig factory;
  Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  EXPECT_CALL(filter_callback, addAccessLogHandler(_));
  cb(filter_callback);
}

} // namespace
} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

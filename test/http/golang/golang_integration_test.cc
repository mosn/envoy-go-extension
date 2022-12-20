#include "envoy/config/bootstrap/v3/bootstrap.pb.h"
#include "envoy/extensions/filters/network/http_connection_manager/v3/http_connection_manager.pb.h"

#include "test/config/v2_link_hacks.h"
#include "test/integration/http_integration.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using Envoy::Http::HeaderValueOf;

namespace Envoy {
namespace {

static const std::string FilterName = "envoy.filters.http.golang";

class GolangIntegrationTest : public testing::TestWithParam<Network::Address::IpVersion>,
                              public HttpIntegrationTest {
public:
  GolangIntegrationTest() : HttpIntegrationTest(Http::CodecType::HTTP1, GetParam()) {}

  void createUpstreams() override {
    HttpIntegrationTest::createUpstreams();
    addFakeUpstream(Http::CodecType::HTTP1);
    addFakeUpstream(Http::CodecType::HTTP1);
    // Create the xDS upstream.
    addFakeUpstream(Http::CodecType::HTTP2);
  }

  void initializeFilter(const std::string& filter_config, const std::string& domain = "*") {
    config_helper_.prependFilter(filter_config);

    // Create static clusters.
    createClusters();

    config_helper_.addConfigModifier(
        [domain](
            envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
                hcm) {
          hcm.mutable_route_config()
              ->mutable_virtual_hosts(0)
              ->mutable_routes(0)
              ->mutable_match()
              ->set_prefix("/test");

          hcm.mutable_route_config()->mutable_virtual_hosts(0)->mutable_routes(0)->set_name(
              "test-route-name");
          hcm.mutable_route_config()->mutable_virtual_hosts(0)->set_domains(0, domain);

          auto* new_route = hcm.mutable_route_config()->mutable_virtual_hosts(0)->add_routes();
          new_route->mutable_match()->set_prefix("/alt/route");
          new_route->mutable_route()->set_cluster("alt_cluster");
          auto* response_header =
              new_route->mutable_response_headers_to_add()->Add()->mutable_header();
          response_header->set_key("fake_header");
          response_header->set_value("fake_value");

          /*
          const std::string key = "envoy.filters.http.golang";
          const std::string yaml =
              R"EOF(
            foo.bar:
              foo: bar
              baz: bat
            keyset:
              foo:
          MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAp0cSZtAdFgMI1zQJwG8ujTXFMcRY0+SA6fMZGEfQYuxcz/e8UelJ1fLDVAwYmk7KHoYzpizy0JIxAcJ+OAE+cd6a6RpwSEm/9/vizlv0vWZv2XMRAqUxk/5amlpQZE/4sRg/qJdkZZjKrSKjf5VEUQg2NytExYyYWG+3FEYpzYyUeVktmW0y/205XAuEQuxaoe+AUVKeoON1iDzvxywE42C0749XYGUFicqBSRj2eO7jm4hNWvgTapYwpswM3hV9yOAPOVQGKNXzNbLDbFTHyLw3OKayGs/4FUBa+ijlGD9VDawZq88RRaf5ztmH22gOSiKcrHXe40fsnrzh/D27uwIDAQAB
          )EOF";

          ProtobufWkt::Struct value;
          TestUtility::loadFromYaml(yaml, value);

          // Sets the route's metadata.
          hcm.mutable_route_config()
              ->mutable_virtual_hosts(0)
              ->mutable_routes(0)
              ->mutable_metadata()
              ->mutable_filter_metadata()
              ->insert(Protobuf::MapPair<std::string, ProtobufWkt::Struct>(key, value));
          */

          // filter level config
          auto vh = hcm.mutable_route_config()->add_virtual_hosts();
          vh->add_domains("filter-level.com");
          vh->set_name("filter-level.com");
          auto* rt = vh->add_routes();
          rt->mutable_match()->set_prefix("/test");
          rt->mutable_route()->set_cluster("cluster_0");

          // virtualhost level config
          const std::string key = "envoy.filters.http.golang";
          const std::string yaml =
              R"EOF(
              "@type": type.googleapis.com/envoy.extensions.filters.http.golang.v3.ConfigsPerRoute
              plugins_config:
                xx:
                  extension_plugin_options:
                    key: 1
                  config:
                    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
                    type_url: typexx
                    value:
                      remove: x-test-header-1
                      set: bar
              )EOF";
          Protobuf::Any value;
          TestUtility::loadFromYaml(yaml, value);
          hcm.mutable_route_config()
              ->mutable_virtual_hosts(0)
              ->mutable_typed_per_filter_config()
              ->insert(Protobuf::MapPair<std::string, Protobuf::Any>(key, value));

          // route level config
          const std::string yaml2 =
              R"EOF(
              "@type": type.googleapis.com/envoy.extensions.filters.http.golang.v3.ConfigsPerRoute
              plugins_config:
                xx:
                  extension_plugin_options:
                    key: 1
                  config:
                    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
                    type_url: typexx
                    value:
                      remove: x-test-header-0
                      set: baz
              )EOF";
          Protobuf::Any value2;
          TestUtility::loadFromYaml(yaml2, value2);

          auto* new_route2 = hcm.mutable_route_config()->mutable_virtual_hosts(0)->add_routes();
          new_route2->mutable_match()->set_prefix("/route-config-test");
          new_route2->mutable_typed_per_filter_config()->insert(
              Protobuf::MapPair<std::string, Protobuf::Any>(key, value2));
          new_route2->mutable_route()->set_cluster("cluster_0");
        });

    initialize();
  }

  void initializeSimpleFilter(const std::string& so_id) {
    addDso(so_id);

    const auto yaml_fmt = R"EOF(
name: golang
typed_config:
  "@type": type.googleapis.com/envoy.extensions.filters.http.golang.v3.Config
  so_id: %s
  plugin_name: xx
  merge_policy: MERGE_VIRTUALHOST_ROUTER_FILTER
  plugin_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: typexx
    value:
      remove: x-test-header-0
      set: foo
)EOF";

    auto yaml_string = absl::StrFormat(yaml_fmt, so_id);
    initializeFilter(yaml_string, "test.com");
  }

  void initializeWithYaml(const std::string& filter_config, const std::string& route_config) {
    config_helper_.prependFilter(filter_config);

    createClusters();
    config_helper_.addConfigModifier(
        [route_config](
            envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
                hcm) { TestUtility::loadFromYaml(route_config, *hcm.mutable_route_config()); });
    initialize();
  }

  void createClusters() {
    config_helper_.addConfigModifier([](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
      auto* golang_cluster = bootstrap.mutable_static_resources()->add_clusters();
      golang_cluster->MergeFrom(bootstrap.static_resources().clusters()[0]);
      golang_cluster->set_name("golang_cluster");

      auto* alt_cluster = bootstrap.mutable_static_resources()->add_clusters();
      alt_cluster->MergeFrom(bootstrap.static_resources().clusters()[0]);
      alt_cluster->set_name("alt_cluster");

      auto* xds_cluster = bootstrap.mutable_static_resources()->add_clusters();
      xds_cluster->MergeFrom(bootstrap.static_resources().clusters()[0]);
      xds_cluster->set_name("xds_cluster");
      ConfigHelper::setHttp2(*xds_cluster);
    });
  }

  std::string genSoPath(std::string name) {
    // TODO: should work without the "_go_extension" suffix
    return TestEnvironment::substitute(
        "{{ test_rundir }}_go_extension/test/http/golang/test_data/" + name + "/filter.so");
  }

  void addDso(const std::string& so_id) {
    const auto yaml_fmt = R"EOF(
name: envoy.bootstrap.dso
typed_config:
  "@type": type.googleapis.com/envoy.extensions.dso.v3.dso
  so_id: %s
  so_path: "%s"
)EOF";
    auto path = genSoPath(so_id);
    auto config = absl::StrFormat(yaml_fmt, so_id, path);

    config_helper_.addBootstrapExtension(config);
  }

  void initializeWithRds(const std::string& filter_config, const std::string& route_config_name,
                         const std::string& initial_route_config) {
    config_helper_.prependFilter(filter_config);

    // Create static clusters.
    createClusters();

    // Set RDS config source.
    config_helper_.addConfigModifier(
        [route_config_name](
            envoy::extensions::filters::network::http_connection_manager::v3::HttpConnectionManager&
                hcm) {
          hcm.mutable_rds()->set_route_config_name(route_config_name);
          hcm.mutable_rds()->mutable_config_source()->set_resource_api_version(
              envoy::config::core::v3::ApiVersion::V3);
          envoy::config::core::v3::ApiConfigSource* rds_api_config_source =
              hcm.mutable_rds()->mutable_config_source()->mutable_api_config_source();
          rds_api_config_source->set_api_type(envoy::config::core::v3::ApiConfigSource::GRPC);
          rds_api_config_source->set_transport_api_version(envoy::config::core::v3::V3);
          envoy::config::core::v3::GrpcService* grpc_service =
              rds_api_config_source->add_grpc_services();
          grpc_service->mutable_envoy_grpc()->set_cluster_name("xds_cluster");
        });

    on_server_init_function_ = [&]() {
      AssertionResult result =
          fake_upstreams_[3]->waitForHttpConnection(*dispatcher_, xds_connection_);
      RELEASE_ASSERT(result, result.message());
      result = xds_connection_->waitForNewStream(*dispatcher_, xds_stream_);
      RELEASE_ASSERT(result, result.message());
      xds_stream_->startGrpcStream();

      EXPECT_TRUE(compareSotwDiscoveryRequest(Config::TypeUrl::get().RouteConfiguration, "",
                                              {route_config_name}, true));
      sendSotwDiscoveryResponse<envoy::config::route::v3::RouteConfiguration>(
          Config::TypeUrl::get().RouteConfiguration,
          {TestUtility::parseYaml<envoy::config::route::v3::RouteConfiguration>(
              initial_route_config)},
          "1");
    };
    initialize();
    registerTestServerPorts({"http"});
  }

  void expectResponseBodyRewrite(const std::string& code, bool empty_body, bool enable_wrap_body) {
    initializeFilter(code);
    codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
    Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                   {":path", "/test"},
                                                   {":scheme", "http"},
                                                   {":authority", "test.com"},
                                                   {"x-forwarded-for", "10.0.0.1"}};

    auto encoder_decoder = codec_client_->startRequest(request_headers);
    Http::StreamEncoder& encoder = encoder_decoder.first;
    auto response = std::move(encoder_decoder.second);
    Buffer::OwnedImpl request_data("done");
    encoder.encodeData(request_data, true);

    waitForNextUpstreamRequest();

    Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}, {"foo", "bar"}};

    if (empty_body) {
      upstream_request_->encodeHeaders(response_headers, true);
    } else {
      upstream_request_->encodeHeaders(response_headers, false);
      Buffer::OwnedImpl response_data1("good");
      upstream_request_->encodeData(response_data1, false);
      Buffer::OwnedImpl response_data2("bye");
      upstream_request_->encodeData(response_data2, true);
    }

    ASSERT_TRUE(response->waitForEndStream());

    if (enable_wrap_body) {
      EXPECT_EQ("2", response->headers()
                         .get(Http::LowerCaseString("content-length"))[0]
                         ->value()
                         .getStringView());
      EXPECT_EQ("ok", response->body());
    } else {
      EXPECT_EQ("", response->body());
    }

    cleanup();
  }

  void testRewriteResponse(const std::string& code) {
    expectResponseBodyRewrite(code, false, true);
  }

  void testRewriteResponseWithoutUpstreamBody(const std::string& code, bool enable_wrap_body) {
    expectResponseBodyRewrite(code, true, enable_wrap_body);
  }

  void testBasic(std::string path) {
    initializeSimpleFilter(BASIC);

    codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
    Http::TestRequestHeaderMapImpl request_headers{
        {":method", "POST"},
        {":path", path},
        {":scheme", "http"},
        {":authority", "test.com"},
        {"x-test-header-0", "foo"},
        {"x-test-header-1", "bar"},
        {"x-test-repeated-header", "foo"},
        {"x-test-repeated-header", "bar"},
    };

    auto encoder_decoder = codec_client_->startRequest(request_headers);
    Http::RequestEncoder& request_encoder = encoder_decoder.first;
    auto response = std::move(encoder_decoder.second);
    codec_client_->sendData(request_encoder, "helloworld", false);
    codec_client_->sendData(request_encoder, "", true);

    waitForNextUpstreamRequest();
    // original header: x-test-header-0
    EXPECT_EQ("foo", upstream_request_->headers()
                         .get(Http::LowerCaseString("x-test-header-0"))[0]
                         ->value()
                         .getStringView());

    // check header value which set in golang: test-x-set-header-0
    EXPECT_EQ("foo", upstream_request_->headers()
                         .get(Http::LowerCaseString("test-x-set-header-0"))[0]
                         ->value()
                         .getStringView());

    // check header exists which removed in golang side: x-test-header-1
    EXPECT_EQ(true,
              upstream_request_->headers().get(Http::LowerCaseString("x-test-header-1")).empty());

    // check handling for repeated headers
    EXPECT_EQ("foobar", upstream_request_->headers()
                            .get(Http::LowerCaseString("test-x-repeated-header"))[0]
                            ->value()
                            .getStringView());
    /*
      * TODO: check route name in decode phase
      EXPECT_EQ("test-route-name", upstream_request_->headers()
                                       .get(Http::LowerCaseString("req-route-name"))[0]
                                       ->value()
                                       .getStringView());
    */

    // "prepend_" + upper("helloworld") + "_append"
    std::string expected = "prepend_HELLOWORLD_append";
    // only match the prefix since data buffer may be combined into a single.
    EXPECT_EQ(expected, upstream_request_->body().toString().substr(0, expected.length()));

    Http::TestResponseHeaderMapImpl response_headers{
        {":status", "200"},
        {"x-test-header-0", "foo"},
        {"x-test-header-1", "bar"},
        {"x-test-repeated-header", "foo"},
        {"x-test-repeated-header", "bar"},
    };
    upstream_request_->encodeHeaders(response_headers, false);
    Buffer::OwnedImpl response_data1("good");
    upstream_request_->encodeData(response_data1, false);
    Buffer::OwnedImpl response_data2("bye");
    upstream_request_->encodeData(response_data2, true);

    ASSERT_TRUE(response->waitForEndStream());

    // original resp header: x-test-header-0
    EXPECT_EQ("foo", response->headers()
                         .get(Http::LowerCaseString("x-test-header-0"))[0]
                         ->value()
                         .getStringView());

    // check resp header value which set in golang: test-x-set-header-0
    EXPECT_EQ("foo", response->headers()
                         .get(Http::LowerCaseString("test-x-set-header-0"))[0]
                         ->value()
                         .getStringView());

    // check resp header exists which removed in golang side: x-test-header-1
    EXPECT_EQ(true, response->headers().get(Http::LowerCaseString("x-test-header-1")).empty());

    // check handling for repeated headers
    EXPECT_EQ("foobar", upstream_request_->headers()
                            .get(Http::LowerCaseString("test-x-repeated-header"))[0]
                            ->value()
                            .getStringView());

    // length("helloworld") = 10
    EXPECT_EQ("10", response->headers()
                        .get(Http::LowerCaseString("test-req-body-length"))[0]
                        ->value()
                        .getStringView());

    // check route name in encode phase
    EXPECT_EQ("test-route-name", response->headers()
                                     .get(Http::LowerCaseString("rsp-route-name"))[0]
                                     ->value()
                                     .getStringView());

    // verify path
    EXPECT_EQ(
        path,
        response->headers().get(Http::LowerCaseString("test-path"))[0]->value().getStringView());

    // upper("goodbye")
    EXPECT_EQ("GOODBYE", response->body());

    cleanup();
  }

  void testRouteConfig(std::string domain, std::string path, bool header_0_existing,
                       std::string set_header) {
    initializeSimpleFilter(ROUTECONFIG);

    codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
    Http::TestRequestHeaderMapImpl request_headers{
        {":method", "GET"}, {":path", path}, {":scheme", "http"}, {":authority", domain}};

    auto encoder_decoder = codec_client_->startRequest(request_headers, true);
    auto response = std::move(encoder_decoder.second);

    waitForNextUpstreamRequest();

    Http::TestResponseHeaderMapImpl response_headers{
        {":status", "200"}, {"x-test-header-0", "foo"}, {"x-test-header-1", "bar"}};
    upstream_request_->encodeHeaders(response_headers, true);

    ASSERT_TRUE(response->waitForEndStream());

    EXPECT_EQ(header_0_existing,
              !response->headers().get(Http::LowerCaseString("x-test-header-0")).empty());

    if (set_header != "") {
      auto values = response->headers().get(Http::LowerCaseString(set_header));
      EXPECT_EQ("test-value", values.empty() ? "" : values[0]->value().getStringView());
    }
    cleanup();
  }

  void testPanicRecover(std::string path) {
    initializeSimpleFilter(BASIC);

    codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
    Http::TestRequestHeaderMapImpl request_headers{
        {":method", "POST"}, {":path", path}, {":scheme", "http"}, {":authority", "test.com"}};

    auto encoder_decoder = codec_client_->startRequest(request_headers);
    Http::RequestEncoder& request_encoder = encoder_decoder.first;
    auto response = std::move(encoder_decoder.second);
    codec_client_->sendData(request_encoder, "hello", false);
    codec_client_->sendData(request_encoder, "world", true);

    // need upstream request then when not seen decode-
    if (path.find("decode-") == std::string::npos) {
      waitForNextUpstreamRequest();
      Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
      upstream_request_->encodeHeaders(response_headers, false);
      Buffer::OwnedImpl response_data1("good");
      upstream_request_->encodeData(response_data1, false);
      Buffer::OwnedImpl response_data2("bye");
      upstream_request_->encodeData(response_data2, true);
    }

    ASSERT_TRUE(response->waitForEndStream());

    // check resp status
    EXPECT_EQ("500", response->headers().getStatusValue());

    // error happened in Go filter\r\n
    auto body = StringUtil::toUpper("error happened in Go filter\r\n");
    EXPECT_EQ(body, StringUtil::toUpper(response->body()));

    cleanup();
  }

  void testSendLocalReply(std::string path, std::string phase) {
    initializeSimpleFilter(BASIC);

    codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
    Http::TestRequestHeaderMapImpl request_headers{
        {":method", "POST"}, {":path", path}, {":scheme", "http"}, {":authority", "test.com"}};

    auto encoder_decoder = codec_client_->startRequest(request_headers);
    Http::RequestEncoder& request_encoder = encoder_decoder.first;
    auto response = std::move(encoder_decoder.second);
    codec_client_->sendData(request_encoder, "hello", false);
    codec_client_->sendData(request_encoder, "world", true);

    // need upstream request then when not seen decode-
    if (path.find("decode-") == std::string::npos) {
      waitForNextUpstreamRequest();
      Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
      upstream_request_->encodeHeaders(response_headers, false);
      Buffer::OwnedImpl response_data1("good");
      upstream_request_->encodeData(response_data1, false);
      Buffer::OwnedImpl response_data2("bye");
      upstream_request_->encodeData(response_data2, true);
    }

    ASSERT_TRUE(response->waitForEndStream());

    // check resp status
    EXPECT_EQ("403", response->headers().getStatusValue());

    // forbidden from go in %s\r\n
    auto body = StringUtil::toUpper(absl::StrFormat("forbidden from go in %s\r\n", phase));
    EXPECT_EQ(body, StringUtil::toUpper(response->body()));

    cleanup();
  }

  void testBufferExceedLimit(std::string path) {
    config_helper_.setBufferLimits(1024, 150);
    initializeSimpleFilter(BASIC);

    codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
    Http::TestRequestHeaderMapImpl request_headers{
        {":method", "POST"}, {":path", path}, {":scheme", "http"}, {":authority", "test.com"}};

    auto encoder_decoder = codec_client_->startRequest(request_headers);
    Http::RequestEncoder& request_encoder = encoder_decoder.first;
    auto response = std::move(encoder_decoder.second);
    // 100 + 200 > 150, excced buffer limit.
    codec_client_->sendData(request_encoder, std::string(100, '-'), false);
    // the two data buffer may be merged into a single buffer, make sure they are two buffer.
    for (auto i = 0; i < 50; i++) {
      codec_client_->connection()->dispatcher().run(Event::Dispatcher::RunType::NonBlock);
    }
    codec_client_->sendData(request_encoder, std::string(200, '-'), true);

    ASSERT_TRUE(response->waitForEndStream());

    // check resp status
    EXPECT_EQ("413", response->headers().getStatusValue());

    auto body = StringUtil::toUpper("payload too large");
    EXPECT_EQ(body, response->body());

    cleanup();
  }

  void cleanup() {
    codec_client_->close();
    if (fake_golang_connection_ != nullptr) {
      AssertionResult result = fake_golang_connection_->close();
      RELEASE_ASSERT(result, result.message());
      result = fake_golang_connection_->waitForDisconnect();
      RELEASE_ASSERT(result, result.message());
    }
    if (fake_upstream_connection_ != nullptr) {
      AssertionResult result = fake_upstream_connection_->close();
      RELEASE_ASSERT(result, result.message());
      result = fake_upstream_connection_->waitForDisconnect();
      RELEASE_ASSERT(result, result.message());
    }
    if (xds_connection_ != nullptr) {
      AssertionResult result = xds_connection_->close();
      RELEASE_ASSERT(result, result.message());
      result = xds_connection_->waitForDisconnect();
      RELEASE_ASSERT(result, result.message());
      xds_connection_ = nullptr;
    }
  }

  FakeHttpConnectionPtr fake_golang_connection_;
  FakeStreamPtr golang_request_;

  const std::string BASIC{"basic"};
  const std::string ASYNC{"async"};
  const std::string ROUTECONFIG{"routeconfig"};
};

INSTANTIATE_TEST_SUITE_P(IpVersions, GolangIntegrationTest,
                         testing::ValuesIn(TestEnvironment::getIpVersionsForTest()),
                         TestUtility::ipTestParamsToString);

TEST_P(GolangIntegrationTest, Basic) { testBasic("/test"); }

TEST_P(GolangIntegrationTest, Async) { testBasic("/test?async=1"); }

TEST_P(GolangIntegrationTest, DataBuffer_DecodeHeader) {
  testBasic("/test?databuffer=decode-header");
}

TEST_P(GolangIntegrationTest, Sleep) { testBasic("/test?sleep=1"); }

TEST_P(GolangIntegrationTest, DataSleep) { testBasic("/test?data_sleep=1"); }

TEST_P(GolangIntegrationTest, Async_Sleep) { testBasic("/test?async=1&sleep=1"); }

TEST_P(GolangIntegrationTest, Async_DataSleep) { testBasic("/test?async=1&data_sleep=1"); }

TEST_P(GolangIntegrationTest, Async_DataBuffer_DecodeHeader) {
  testBasic("/test?async=1&databuffer=decode-header");
}

TEST_P(GolangIntegrationTest, DataBuffer_DecodeData) { testBasic("/test?databuffer=decode-data"); }

TEST_P(GolangIntegrationTest, Async_DataBuffer_DecodeData) {
  testBasic("/test?async=1&databuffer=decode-data");
}

TEST_P(GolangIntegrationTest, LocalReply_DecodeHeader) {
  testSendLocalReply("/test?localreply=decode-header", "decode-header");
}

TEST_P(GolangIntegrationTest, LocalReply_DecodeHeader_Async) {
  testSendLocalReply("/test?async=1&localreply=decode-header", "decode-header");
}

TEST_P(GolangIntegrationTest, LocalReply_DecodeData) {
  testSendLocalReply("/test?localreply=decode-data", "decode-data");
}

TEST_P(GolangIntegrationTest, LocalReply_DecodeData_Async) {
  testSendLocalReply("/test?async=1&sleep=1&localreply=decode-data", "decode-data");
}

/*
TODO: test failed due to memory leak...
TEST_P(GolangIntegrationTest, LocalReply_EncodeHeader_Async) {
  testSendLocalReply("/test?async=1&sleep=1&localreply=encode-header", "encode-header");
}

TEST_P(GolangIntegrationTest, LocalReply_EncodeData_Async) {
  testSendLocalReply("/test?async=1&sleep=1&localreply=encode-data", "encode-data");
}

// dual sendLocalReply
TEST_P(GolangIntegrationTest, LocalReply_DecodeData_EncodeHeader_Async) {
  testSendLocalReply("/test?async=1&sleep=1&localreply=decode-data,encode-header", "encode-header");
}

TEST_P(GolangIntegrationTest, LocalReply_DecodeData_EncodeData_Async) {
  testSendLocalReply("/test?async=1&sleep=1&localreply=decode-data,encode-data", "encode-data");
}
*/

TEST_P(GolangIntegrationTest, LuaRespondAfterGoHeaderContinue) {
  // put lua filter after golang filter
  // golang filter => lua filter.

  const std::string LUA_RESPOND =
      R"EOF(
name: envoy.filters.http.lua
typed_config:
  "@type": type.googleapis.com/envoy.extensions.filters.http.lua.v3.Lua
  inline_code: |
    function envoy_on_request(handle)
      local orig_header = handle:headers():get('x-test-header-0')
      local go_header = handle:headers():get('test-x-set-header-0')
      handle:respond({[":status"] = "403"}, "forbidden from lua, orig header: "
        .. (orig_header or "nil")
        .. ", go header: "
        .. (go_header or "nil"))
    end
)EOF";
  config_helper_.prependFilter(LUA_RESPOND);

  initializeSimpleFilter(BASIC);

  codec_client_ = makeHttpConnection(makeClientConnection(lookupPort("http")));
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":path", "/test"},
                                                 {":scheme", "http"},
                                                 {":authority", "test.com"},
                                                 {"x-test-header-0", "foo"}};

  auto encoder_decoder = codec_client_->startRequest(request_headers);
  Http::RequestEncoder& request_encoder = encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  codec_client_->sendData(request_encoder, "hello", true);

  ASSERT_TRUE(response->waitForEndStream());

  // check resp status
  EXPECT_EQ("403", response->headers().getStatusValue());

  // forbidden from lua, orig header: foo, go header: foo
  auto body = StringUtil::toUpper("forbidden from lua, orig header: foo, go header: foo");
  EXPECT_EQ(body, response->body());

  cleanup();
}

TEST_P(GolangIntegrationTest, BufferExceedLimit_DecodeHeader) {
  testBufferExceedLimit("/test?databuffer=decode-header");
}

/*
TODO: This test case it not stable
TEST_P(GolangIntegrationTest, BufferExceedLimit_DecodeData) {
  testBufferExceedLimit("/test?databuffer=decode-data");
}
*/

// config in filter
// remove: x-test-header-0
// set: foo
TEST_P(GolangIntegrationTest, RouteConfig_Filter) {
  testRouteConfig("filter-level.com", "/test", false, "foo");
}

// config in virtualhost
// remove: x-test-header-1
// set: bar
TEST_P(GolangIntegrationTest, RouteConfig_VirtualHost) {
  testRouteConfig("test.com", "/test", true, "bar");
}

// config in route
// remove: x-test-header-0
// set: baz
TEST_P(GolangIntegrationTest, RouteConfig_Route) {
  testRouteConfig("test.com", "/route-config-test", false, "baz");
}

TEST_P(GolangIntegrationTest, PanicRecover_DecodeHeader) {
  testPanicRecover("/test?panic=decode-header");
}

TEST_P(GolangIntegrationTest, PanicRecover_DecodeHeader_Async) {
  testPanicRecover("/test?async=1&panic=decode-header");
}

TEST_P(GolangIntegrationTest, PanicRecover_DecodeData) {
  testPanicRecover("/test?panic=decode-data");
}

TEST_P(GolangIntegrationTest, PanicRecover_DecodeData_Async) {
  testPanicRecover("/test?async=1&sleep=1&panic=decode-data");
}

TEST_P(GolangIntegrationTest, PanicRecover_EncodeData_Async) {
  testPanicRecover("/test?async=1&sleep=1&panic=encode-data");
}

} // namespace
} // namespace Envoy

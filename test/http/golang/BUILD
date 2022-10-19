load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
    "envoy_cc_test",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//src/envoy/http/golang:config",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/test_common:utility_lib",
        "//api/http/golang/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "golang_filter_test",
    srcs = ["golang_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/stream_info:stream_info_lib",
        "//src/envoy/http/golang:golang_filter_lib",
        "@envoy//test/mocks/api:api_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/network:network_mocks",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/mocks/ssl:ssl_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/mocks/upstream:cluster_manager_mocks",
        "@envoy//test/test_common:logging_lib",
        "@envoy//test/test_common:test_runtime_lib",
        "@envoy//test/test_common:utility_lib",
        "//api/http/golang/v3:pkg_cc_proto",
    ],
)
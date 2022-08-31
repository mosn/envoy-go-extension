package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        # "//src/meta_protocol_proxy:config",
        # "//src/application_protocols/dubbo:config",
        # "//src/application_protocols/thrift:config",
        # "//src/application_protocols/brpc:config",
        "//src/common/dso:dso_lib",
        "//src/http/golang:config",
        # skipped due to this error:
        # ERROR: /home/zhudejiang.pt/.cache/bazel/_bazel_zhudejiang.pt/e6d51174313f6fcb2790deca9a35220d/external/io_istio_proxy/extensions/metadata_exchange/BUILD:11:17: in cc_library rule @io_istio_proxy//extensions/metadata_exchange:metadata_exchange_lib: target '@envoy//source/extensions/common/wasm/ext:declare_property_cc_proto' is not visible from target '@io_istio_proxy//extensions/metadata_exchange:metadata_exchange_lib'. Check the visibility declaration of the former target if you think the dependency is legitimate
        # "@io_istio_proxy//extensions/access_log_policy:access_log_policy_lib",
        # "@io_istio_proxy//extensions/attributegen:attributegen_plugin",
        # "@io_istio_proxy//extensions/metadata_exchange:metadata_exchange_lib",
        # "@io_istio_proxy//extensions/stackdriver:stackdriver_plugin",
        # "@io_istio_proxy//extensions/stats:stats_plugin",
        # "@envoy//source/extensions/common/wasm:wasm_lib",
        # "@io_istio_proxy//src/envoy/http/alpn:config_lib",
        # "@io_istio_proxy//src/envoy/http/authn:filter_lib",
        # "@io_istio_proxy//src/envoy/tcp/forward_downstream_sni:config_lib",
        # "@io_istio_proxy//src/envoy/tcp/metadata_exchange:config_lib",
        # "@io_istio_proxy//src/envoy/tcp/sni_verifier:config_lib",
        # "@io_istio_proxy//src/envoy/tcp/tcp_cluster_rewrite:config_lib",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

licenses(["notice"])  # Apache 2

envoy_cc_library(
    name = "golang_cluster_lib",
    srcs = [
        "golang_cluster.cc",
    ],
    hdrs = [
        "golang_cluster.h",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/common:enum_to_int",
        "@envoy//source/common/common:utility_lib",
        "@envoy//source/common/grpc:context_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/common:linked_object",
        "@envoy//source/common/router:config_lib",
        "//src/envoy/common/dso:dso_lib",
        "//api/http/cluster/v3:pkg_cc_proto",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":golang_cluster_lib",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/router:cluster_specifier_plugin_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "//src/envoy/common/dso:dso_lib",
        "//api/http/cluster/v3:pkg_cc_proto",
    ],
)

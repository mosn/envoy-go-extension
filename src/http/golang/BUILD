package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

licenses(["notice"])  # Apache 2

# Golang extensions filter.
# Public docs: docs/root/configuration/http_filters/golang_extension_filter.rst

envoy_cc_library(
    name = "golang_extension_filter_lib",
    srcs = ["golang_extension_filter.cc"],
    hdrs = ["golang_extension_filter.h"],
    repository = "@envoy",
    deps = [
        "@envoy//envoy/http:codes_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//source/common/common:enum_to_int",
        "@envoy//source/common/common:utility_lib",
        "@envoy//source/common/grpc:context_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http/http1:codec_lib",
        "//src/common/dso:dso_lib",
        "//api/http/golang/v3:pkg_cc_proto",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http:well_known_names",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "//src/http/golang:golang_extension_filter_lib",
        "//src/common/dso:dso_lib",
        "//api/http/golang/v3:pkg_cc_proto",
    ],
)

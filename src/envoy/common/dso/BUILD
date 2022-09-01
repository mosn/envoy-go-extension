package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

licenses(["notice"])  # Apache 2

envoy_cc_library(
    name = "dso_lib",
    srcs = ["dso.cc"],
    repository = "@envoy",
    hdrs = [
        "api.h",
        "dso.h",
        "libgolang.h",
    ],
    deps = [
        "@envoy//source/common/common:minimal_logger_lib",
    ],
)

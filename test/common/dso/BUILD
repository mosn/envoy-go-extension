load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
    "envoy_cc_test",
)
load("//:bazel/dso.bzl", "envoy_select_golang_tests")

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "dso_test",
    srcs = ["dso_test.cc"],
    repository = "@envoy",
    data = envoy_select_golang_tests([
        "//test/common/dso/test_data:simple.so",
    ]),
    deps = [
        "//src/envoy/common/dso:dso_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
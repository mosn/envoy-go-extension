load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "api",
    srcs = [
        "capi.go",
        "filter.go",
        "type.go",
    ],
    importpath = "mosn.io/envoy-go-extension/pkg/http/api",
    visibility = ["//visibility:public"],
)

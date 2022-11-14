load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "api",
    srcs = [
        "capi.go",
        "filter.go",
        "type.go",
    ],
    importpath = "mosn.io/envoy-go-extension/pkg/api",
    visibility = ["//visibility:public"],
    deps = [
        "@org_golang_google_protobuf//types/known/anypb",
    ],
)

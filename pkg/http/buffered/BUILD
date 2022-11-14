load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "buffered",
    srcs = [
        "api.go",
        "filtermanager.go",
    ],
    importpath = "mosn.io/envoy-go-extension/pkg/http/buffered",
    visibility = ["//visibility:public"],
    deps = [
        "//pkg/api",
        "//pkg/http",
    ],
)

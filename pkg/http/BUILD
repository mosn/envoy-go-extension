load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "http",
    srcs = [
        "api.h",
        "capi.go",
        "config.go",
        "filter.go",
        "filtermanager.go",
        "moe.go",
        "passthrough.go",
        "type.go",
    ],
    cgo = True,
    clinkopts = select({
        "@io_bazel_rules_go//go/platform:android": [
            "-Wl,-unresolved-symbols=ignore-all",
        ],
        "@io_bazel_rules_go//go/platform:darwin": [
            "-Wl,-undefined,dynamic_lookup",
        ],
        "@io_bazel_rules_go//go/platform:ios": [
            "-Wl,-undefined,dynamic_lookup",
        ],
        "@io_bazel_rules_go//go/platform:linux": [
            "-Wl,-unresolved-symbols=ignore-all",
        ],
        "//conditions:default": [],
    }),
    importpath = "mosn.io/envoy-go-extension/pkg/http",
    visibility = ["//visibility:public"],
    deps = [
        "//pkg/api",
        "//pkg/utils",
        "@org_golang_google_protobuf//proto",
        "@org_golang_google_protobuf//types/known/anypb",
        "@org_golang_google_protobuf//types/known/structpb",
    ],
)

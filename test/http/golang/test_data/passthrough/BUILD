load("//:bazel/dso.bzl", "go_shared_so")
load("@io_bazel_rules_go//go:def.bzl", "go_library")

go_library(
    name = "filter_lib",
    srcs = [
        "moe.go",
    ],
    importpath = "mosn.io/",
)

go_shared_so(
    name = "filter.so",
    srcs = [
        "filter.go",
    ],
)

sh_binary(
    name = "compile",
    srcs = ["script.sh"],
    data = [
        ":filter.go",
        ":go.mod",
    ]
)

genrule(
    name = "run_script",
    srcs = [
        "script.sh",
        "filter.go",
        "go.mod",
        "go.sum",
        "http",
        "utils",
    ],
    outs = [
        "libfilter.so",
    ],
    cmd = "$(location script.sh)",
)
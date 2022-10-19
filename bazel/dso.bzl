load("@io_bazel_rules_go//go:def.bzl", "go_binary")

# Selects the given values depending on the golang cgo enabled in the current build.
def envoy_select_golang_tests(xs):
    # TODO: check cgo enabled
    return select({
        "//conditions:default": xs,
    })

def go_shared_so(name, **kwargs):
    go_binary(
        name = name,
        out = name,
        cgo = True,
        linkmode = "c-shared",
        visibility = ["//visibility:public"],
        **kwargs
    )

def go_filter_so(**kwargs):
    go_shared_so(
        name = "filter.so",
        srcs = [
            "filter.go",
        ],
        deps = [
          "@io_mosn_envoy_go_extension//pkg/http:http",
          "@io_mosn_envoy_go_extension//pkg/http/api:api",
          "@io_mosn_envoy_go_extension//pkg/http/buffered:buffered",
          "@io_mosn_envoy_go_extension//pkg/utils:utils",
        ],
        **kwargs
    )

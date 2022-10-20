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
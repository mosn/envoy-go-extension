# envoy-go-extension

Implement Envoy filter by using Go language.

1. support full-featured Go language, including goroutine.
2. support streaming, with rarely worry about concurrency conflict.

<!--ts-->
* [envoy-go-extension](#envoy-go-extension)
   * [Status](#status)
   * [API](#api)
   * [Design](#design)
      * [Memory Safety](#memory-safety)
      * [Concurrency Safety](#concurrency-safety)
      * [Sandbox Safety](#sandbox-safety)
   * [Build](#build)
      * [Envoy](#envoy)
      * [golang shared object](#golang-shared-object)
   * [Run](#run)
   * [Test](#test)
<!--te-->

## Status

This project is under heavy active development at this stage.

## API

coming soon.

## Design

### Memory Safety

coming soon.

### Concurrency Safety

coming soon.

### Sandbox Safety

coming soon.

## Build

### Envoy

1. Install Bazel

See details in [envoyproxy](https://github.com/envoyproxy/envoy/blob/main/bazel/README.md)

2. Build Envoy

Generate executable envoy file under bazel-bin.

Note: only support gcc yet. [clang support in TODO](https://github.com/mosn/envoy-go-extension/issues/19).

```
# use default build options
make build-envoy

# add more build options
build_opts=""
make build-envoy BUILD_OPTS=$build_opts
```

### golang shared object

Generate libgolang.so under the current directory.

```
# using local go command, recommended when you are using linux.
make build-so-local

# using docker
make build-so
```

## Run

Please update the `so_path` field (in envoy-golang.yaml) to you local path.

```
# it will use the envoy-golang.yaml config file.
make run
```

## Test

```
# use default build options for testing
make test-envoy

# add more build options for testing
build_opts=""
make build-envoy BUILD_OPTS=$build_opts
```

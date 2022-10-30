# envoy-go-extension

Implement Envoy filter by using Go language.

1. support full-featured Go language, including goroutine.
2. support streaming, with rarely worry about concurrency conflict.

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

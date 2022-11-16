## envoy-go-extension

Implement Envoy filter by using Go.

1. support full-featured Go language, including goroutine.
2. support streaming, with rarely worry about concurrency conflict.

## Table of Contents

<!--ts-->
   * [envoy-go-extension](#envoy-go-extension)
   * [Status](#status)
   * [API](#api)
   * [Requirements](#requirements)
   * [Samples](#samples)
   * [Build](#build)
      * [Envoy](#envoy)
      * [golang shared object](#golang-shared-object)
   * [Run](#run)
   * [Test](#test)
   * [Design](#design)
      * [Memory Safety](#memory-safety)
      * [Concurrency Safety](#concurrency-safety)
         * [Go side](#go-side)
         * [Envoy side](#envoy-side)
      * [Sandbox Safety](#sandbox-safety)
<!--te-->

## Status

This project is under heavy active development at this stage.

## API

coming soon.

## Requirements

1. Go >= 1.18
2. Gcc >= 9 or Clang >= 11.1.0
3. Bazel >= 5.1.1

## Samples

If you just want to use this extension. Please refer to [envoy-go-filter-samples](https://github.com/mosn/envoy-go-filter-samples).

It shows how to implement a filter by using pure Go.

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
build_opts="--define=wasm=disabled"
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

## Design

### Memory Safety

Users use Go as normal, do not need to care about memory safety.

1. Go objects from these APIs, come from Go GC manager.
2. Copy memory from envoy on demand, for performance.
3. Go could only touch Envoy memory before OnDestroy.

### Concurrency Safety

#### Go side

We support async(goroutine) in the Go side, and streaming. But, users do not need to care about the conncurrency in go side.

We buffer the data (with watermark) in Envoy (C++) side, until the Go returns. So, there is no concurrency in Go side, except `OnDestory`.

#### Envoy side

Go code may run in Go thread (not the safe enovy thread), since we support goroutine.

In honesty, this breaks the Enovy thread-safe model.
But, users do not need to care about if the code is running Go thread or not.

1. For `Data`

we always consume data from `filtermanager` to private `BufferInstance`, so there won't be concurrency conflict between Go and Envoy, even without lock.

2. For `HeaderMap` and `TrailerMap`

Now, no lock with optimism.

Go only read Envoy memory in Go thread, before the headers or trailers continue to the next filter.
Yeah, We are optimistic that other filters will not write headers or trailers when they are not processing them.

Write operations still happens in Envoy safe thread.

If we are proven wrong, we will add lock for them.

### Sandbox Safety

Go code may cause panic, leads to breaking down the whole Envoy process, when there is dangerous code. i.e. concurrent write on a map.

Now, the users should care about the risk, align to users develop a filter using C++, or users develop a service using Go.

In the feature, we may provide a Go release which support sandbox safety.

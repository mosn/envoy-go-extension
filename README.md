# envoy-go-extension

This project is under heavy active development at this stage.

## API

coming soon.

## Desgin

coming soon.

1. Memory Safety
2. Concurrency Safety
3. Sandbox Safety

## Build

### Enovy

1. Install Bazel

See details in https://github.com/envoyproxy/envoy/blob/main/bazel/README.md.

2. Build Envoy

```
make build-envoy
```

### golang shared object

```
# using local go command, recommended when you are using linux.
make build-so-local

# using docker
make build-so
```
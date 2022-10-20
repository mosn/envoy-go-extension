
BUILD_IMAGE  = golang:1.14.13
PROJECT_NAME = mosn.io/envoy-go-extension

# go so

build-so-local:
	go build \
		-v \
		--buildmode=c-shared \
		-o libgolang.so \
		.

build-so:
	docker run --rm -v $(shell pwd):/go/src/${PROJECT_NAME} -w /go/src/${PROJECT_NAME} ${BUILD_IMAGE} make build-so-local

sync-headers:
	cp libgolang.h ../envoy/common/dso/
	cp http/api.h ../envoy/common/dso/

.PHONY: build-so-local, build-so, sync-headers

# envoy extension

image:
	# bazel-bin is a soft link
	cp -f bazel-bin/envoy envoy
	docker build --no-cache -t envoy-go-extension .


.PHONY: build clean api

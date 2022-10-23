
BUILD_IMAGE  = golang:1.14.13
PROJECT_NAME = mosn.io/envoy-go-extension

COMPILE_MODE = dbg
COPTS = --copt "-Wno-stringop-overflow" \
		--copt "-Wno-vla-parameter" \
		--copt "-Wno-parentheses" \
		--copt "-Wno-unused-parameter" \
		--copt "-Wno-range-loop-construct"
TARGET = "//:envoy"
# more custom options
BUILD_OPTS =

# go so

build-so-local:
	go build \
		-v \
		--buildmode=c-shared \
		-o libgolang.so \
		.

build-so:
	docker run --rm -v $(shell pwd):/go/src/${PROJECT_NAME} -w /go/src/${PROJECT_NAME} ${BUILD_IMAGE} make build-so-local

check-test-data-compile:
	./scripts/check-test-data-compile.sh

sync-headers:
	# export header by cgo tool
	cd pkg/http \
		&& go tool cgo --exportheader libgolang.h moe.go \
		&& cd ../../
	cp pkg/http/libgolang.h src/envoy/common/dso/
	cp pkg/http/api.h src/envoy/common/dso/

.PHONY: build-so-local, build-so, sync-headers, check-test-data-compile

# envoy extension

build-envoy:
	bazel build \
		-c ${COMPILE_MODE} \
		${COPTS} \
		--linkopt=-Wl,--dynamic-list=$(shell pwd)/export-syms.txt \
		${TARGET} \
			--verbose_failures \
			${BUILD_OPTS}

test:
	echo ${COMPILE_MODE}
	echo ${BUILD_OPTS}
	echo ${COPTS}

image:
	# bazel-bin is a soft link
	cp -f bazel-bin/envoy envoy
	docker build --no-cache -t envoy-go-extension .


.PHONY: build-envoy image test

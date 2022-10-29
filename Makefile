
BUILD_IMAGE  = golang:1.14.13
PROJECT_NAME = mosn.io/envoy-go-extension

COMPILE_MODE = dbg
COPTS = --copt "-Wno-stringop-overflow" \
		--copt "-Wno-vla-parameter" \
		--copt "-Wno-parentheses" \
		--copt "-Wno-unused-parameter" \
		--copt "-Wno-range-loop-construct"
TARGET = "//:envoy"
TEST_TARGET = "//test/..."
TEST_LOG_LEVEL = debug
# more custom options
BUILD_OPTS =

# go so
.PHONY: build-so-local, build-so, sync-headers, check-test-data-compile

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

# envoy extension
.PHONY: build-envoy test-envoy

build-envoy:
	bazel build \
		-c ${COMPILE_MODE} \
		${COPTS} \
		--linkopt=-Wl,--dynamic-list=$(shell pwd)/export-syms.txt \
		${TARGET} \
			--verbose_failures \
			${BUILD_OPTS}

test-envoy:
	bazel test \
		${COPTS} \
		--linkopt=-Wl,--dynamic-list=$(shell pwd)/export-syms.txt \
		${TEST_TARGET} \
			--test_arg="-l ${TEST_LOG_LEVEL}" \
			--test_env=ENVOY_IP_TEST_VERSIONS=v4only \
			--test_env=GODEBUG=cgocheck=0 \
			--test_verbose_timeout_warnings \
			--verbose_failures \
			${BUILD_OPTS}


.PHONY: image
image:
	# bazel-bin is a soft link
	cp -f bazel-bin/envoy envoy
	docker build --no-cache -t envoy-go-extension .

.PHONY: run
run:
	GODEBUG=cgocheck=0 \
	./bazel-bin/envoy \
		-c envoy-golang.yaml \
		-l debug \
		--base-id 1

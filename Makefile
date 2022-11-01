
BUILD_IMAGE  = golang:1.14.13
PROJECT_NAME = mosn.io/envoy-go-extension

COPTS = --config=gcc \
		--copt "-Wno-stringop-overflow" \
		--copt "-Wno-vla-parameter" \
		--copt "-Wno-parentheses" \
		--copt "-Wno-unused-parameter" \
		--copt "-Wno-range-loop-construct" \
		--copt "-Wno-uninitialized" \
		--copt "-Wno-strict-aliasing" \
		--copt "-fno-strict-aliasing"

COMPILE_MODE = dbg
TARGET = "//:envoy"

TEST_COMPILE_MODE = fastbuild
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
		${TARGET} \
			--verbose_failures \
			${BUILD_OPTS}

test-envoy:
	bazel test \
		-c ${TEST_COMPILE_MODE} \
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
image: build-envoy build-so-local
	# bazel-bin is a soft link
	cp -f bazel-bin/envoy envoy
	sudo docker build --no-cache -t envoy-go-extension .
	sudo docker tag envoy-go-extension:latest mosnio/envoy-go-extension:latest
	sudo docker push mosnio/envoy-go-extension:latest


.PHONY: run
run:
	GODEBUG=cgocheck=0 \
	./bazel-bin/envoy \
		-c envoy-golang.yaml \
		-l debug \
		--base-id 1

.PHONY: gen-toc
gen-toc:
	# gh-md-toc from https://github.com/ekalinin/github-markdown-toc
	gh-md-toc --insert --no-backup --hide-footer README.md
	sed -i '/#table-of-contents/d' README.md

# update go_repository in WORKSPACE
.PHONY: update-repos
update-repos:
	gazelle update-repos -from_file=go.mod

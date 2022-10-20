image:
	# bazel-bin is a soft link
	cp -f bazel-bin/envoy envoy
	sudo docker build --no-cache -t envoy-go-extension .

build-so-local:
	go build \
		-v \
		--buildmode=c-shared \
		-o libgolang.so \
		.

.PHONY: build clean api

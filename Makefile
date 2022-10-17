image:
	# bazel-bin is a soft link
	cp -f bazel-bin/envoy envoy
	sudo docker build --no-cache -t envoy-go-extension .

.PHONY: build clean api

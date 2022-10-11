
build:
	go build \
		-v \
		--buildmode=c-shared \
		-o libgolang.so \
		.
	cp libgolang.h ../envoy/common/dso/
	cp http/api.h ../envoy/common/dso/

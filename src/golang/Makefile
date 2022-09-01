
build:
	go build \
		-v \
		--buildmode=c-shared \
		-o libgolang.so \
		.
	cp libgolang.h ../envoy/common/dso/
	cp api.h ../envoy/common/dso/

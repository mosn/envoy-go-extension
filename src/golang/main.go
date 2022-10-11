package main

import (
	"mosn.io/envoy-go-extension/http"
	"mosn.io/envoy-go-extension/http/buffered"
	"mosn.io/envoy-go-extension/samples/async"
	bufferedSample "mosn.io/envoy-go-extension/samples/buffered"
)

func init() {
	http.RegisterHttpFilterConfigFactory(async.ConfigFactory)
	buffered.RegisterHttpFilterConfigFactory(bufferedSample.ConfigFactory)
}

func main() {
}

package main

import (
	"mosn.io/envoy-go-extension/sdk/http"
	"mosn.io/envoy-go-extension/sdk/http/buffered"
	"mosn.io/envoy-go-extension/sdk/samples/async"
	asyncSleep "mosn.io/envoy-go-extension/sdk/samples/async-sleep"
	bufferedSample "mosn.io/envoy-go-extension/sdk/samples/buffered"
)

func init() {
	buffered.RegisterHttpFilterConfigFactory(bufferedSample.ConfigFactory)
	http.RegisterHttpFilterConfigFactory(asyncSleep.ConfigFactory)
	http.RegisterHttpFilterConfigFactory(async.ConfigFactory)
	http.RegisterHttpFilterConfigFactory(http.PassThroughFactory)
	buffered.RegisterHttpFilterConfigFactory(bufferedSample.ConfigFactory)
}

func main() {
}

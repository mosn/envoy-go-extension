package main

import (
	"mosn.io/envoy-go-extension/pkg/http"
	"mosn.io/envoy-go-extension/pkg/http/buffered"
	"mosn.io/envoy-go-extension/pkg/samples/async"
	asyncSleep "mosn.io/envoy-go-extension/pkg/samples/async-sleep"
	bufferedSample "mosn.io/envoy-go-extension/pkg/samples/buffered"
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

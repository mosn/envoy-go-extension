package main

import (
	"mosn.io/envoy-go-extension/pkg/http"
	"mosn.io/envoy-go-extension/pkg/http/buffered"
	"mosn.io/envoy-go-extension/samples/async"
	asyncSleep "mosn.io/envoy-go-extension/samples/async-sleep"
	bufferedSample "mosn.io/envoy-go-extension/samples/buffered"
)

func init() {
	// these are useless samples
	buffered.RegisterHttpFilterConfigFactory(bufferedSample.ConfigFactory)
	http.RegisterHttpFilterConfigFactory(asyncSleep.ConfigFactory)
	http.RegisterHttpFilterConfigFactory(async.ConfigFactory)
	buffered.RegisterHttpFilterConfigFactory(bufferedSample.ConfigFactory)

	// default filter
	http.RegisterHttpFilterConfigFactory(http.PassThroughFactory)
}

func main() {
}

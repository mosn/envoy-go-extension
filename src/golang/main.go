package main

import (
	"mosn.io/envoy-go-extension/http"
	"mosn.io/envoy-go-extension/samples/async"
)

func init() {
	http.RegisterHttpFilterConfigFactory(async.ConfigFactory)
}

func main() {
}

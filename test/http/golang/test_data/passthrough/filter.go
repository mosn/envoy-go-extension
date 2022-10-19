package main

import "mosn.io/envoy-go-extension/pkg/http"

func init() {
	http.RegisterHttpFilterConfigFactory(http.PassThroughFactory)
}

func main() {
}

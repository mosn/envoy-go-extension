package http

import (
	"mosn.io/envoy-go-extension/http/api"
)

// pass through by default
var httpFilterConfigFactory api.HttpFilterConfigFactory = passThroughFactory

func RegisterHttpFilterConfigFactory(f api.HttpFilterConfigFactory) {
	httpFilterConfigFactory = f
}

func getOrCreateHttpFilterFactory(configId uint64) api.HttpFilterFactory {
	config, ok := configCache[configId]
	if !ok {
		// TODO: panic
	}
	return httpFilterConfigFactory(config)
}

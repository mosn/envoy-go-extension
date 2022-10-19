package buffered

import (
	"mosn.io/envoy-go-extension/sdk/http/api"
	"mosn.io/envoy-go-extension/sdk/http/buffered"
)

type filter struct {
	config    interface{}
	callbacks api.FilterCallbackHandler
}

func (f *filter) Decode(header api.RequestHeaderMap, data api.BufferInstance, trailer api.RequestTrailerMap) {
}
func (f *filter) Encode(header api.ResponseHeaderMap, data api.BufferInstance, trailer api.ResponseTrailerMap) {
	if data != nil {
		data.Set("Foo-Bar")
		header.Set("Content-Length", "7")
	}
	header.Set("foo", "bar")
	if trailer != nil {
		trailer.Set("t1", "v1")
	}
}

func ConfigFactory(config interface{}) buffered.HttpFilterFactory {
	return func(callbacks api.FilterCallbackHandler) buffered.HttpFilter {
		return &filter{
			config:    config,
			callbacks: callbacks,
		}
	}
}

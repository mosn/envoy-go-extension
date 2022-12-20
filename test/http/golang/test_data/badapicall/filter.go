package main

import (
	"mosn.io/envoy-go-extension/pkg/api"
	"mosn.io/envoy-go-extension/pkg/http"
)

func init() {
	http.RegisterHttpFilterConfigFactory(configFactory)
}

type filter struct {
	callbacks api.FilterCallbackHandler
	headers   api.RequestHeaderMap
}

func (f *filter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	f.headers = header
	return api.Continue
}

func (f *filter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	// should panic get header after header continued
	f.headers.Path()
	return api.Continue
}

func (f *filter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) OnDestroy(reason api.DestroyReason) {
}

func configFactory(interface{}) api.HttpFilterFactory {
	return func(callbacks api.FilterCallbackHandler) api.HttpFilter {
		return &filter{
			callbacks: callbacks,
		}
	}
}

func main() {
}

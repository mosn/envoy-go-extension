package main

import (
	"mosn.io/envoy-go-extension/pkg/http"
	"mosn.io/envoy-go-extension/pkg/http/api"
	"strconv"
)

func init() {
	http.RegisterHttpFilterConfigFactory(configFactory)
}

type filter struct {
	callbacks       api.FilterCallbackHandler
	req_body_length uint64
}

func (f *filter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	header.Set("test_set_header_0", header.Get("x-test-header"))
	return api.Continue
}

func (f *filter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	f.req_body_length += buffer.Length()
	buffer.Set(buffer.GetString())
	return api.Continue
}

func (f *filter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	header.Set("test_set_header_0", header.Get("foo"))
	header.Set("test-req-body-length", strconv.Itoa(int(f.req_body_length)))
	return api.Continue
}

func (f *filter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	buffer.Set(buffer.GetString())
	return api.Continue
}

func (f *filter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) OnDestroy(reason api.DestroyReason) {
	// fmt.Printf("OnDestory, reason: %d\n", reason)
}

func (f *filter) Callbacks() api.FilterCallbacks {
	return f.callbacks
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

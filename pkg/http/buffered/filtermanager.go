package buffered

import (
	"mosn.io/envoy-go-extension/pkg/http"
	"mosn.io/envoy-go-extension/pkg/http/api"
)

// TODO: support filter chain, we only register one now
var httpFilterConfigFactory HttpFilterConfigFactory

// full buffered
func RegisterHttpFilterConfigFactory(f HttpFilterConfigFactory) {
	httpFilterConfigFactory = f
	// make sure we registered the raw factory
	http.RegisterHttpFilterConfigFactory(configFactory)
}

type bufferedFilter struct {
	config    interface{}
	callbacks api.FilterCallbacks
	reqHeader api.RequestHeaderMap
	reqData   api.BufferInstance
	rspHeader api.ResponseHeaderMap
	rspData   api.BufferInstance

	// TODO: filter chain
	filter HttpFilter
}

func (f *bufferedFilter) runDecode(header api.RequestHeaderMap, data api.BufferInstance, trailer api.RequestTrailerMap) {
	go func() {
		f.filter.Decode(header, data, trailer)
		f.callbacks.Continue(api.Continue)
	}()
}

func (f *bufferedFilter) runEncode(header api.ResponseHeaderMap, data api.BufferInstance, trailer api.ResponseTrailerMap) {
	go func() {
		f.filter.Encode(header, data, trailer)
		f.callbacks.Continue(api.Continue)
	}()
}

func (f *bufferedFilter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	if endStream {
		f.runDecode(header, nil, nil)
		return api.Running
	}
	f.reqHeader = header
	return api.StopAndBuffer
}

func (f *bufferedFilter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	if endStream {
		f.runDecode(f.reqHeader, buffer, nil)
		return api.Running
	}
	f.reqData = buffer
	return api.StopAndBuffer
}

func (f *bufferedFilter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	f.runDecode(f.reqHeader, f.reqData, trailers)
	return api.Running
}

func (f *bufferedFilter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	if endStream {
		f.runEncode(header, nil, nil)
		return api.Running
	}
	f.rspHeader = header
	return api.StopAndBuffer
}

func (f *bufferedFilter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	if endStream {
		f.runEncode(f.rspHeader, buffer, nil)
		return api.Running
	}
	f.rspData = buffer
	return api.StopAndBuffer
}

func (f *bufferedFilter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	f.runEncode(f.rspHeader, f.rspData, trailers)
	return api.Running
}

func (f *bufferedFilter) OnDestroy(reason api.DestroyReason) {
	// fmt.Printf("OnDestory, reason: %d\n", reason)
}

func configFactory(config interface{}) api.HttpFilterFactory {
	return func(callbacks api.FilterCallbackHandler) api.HttpFilter {
		// TODO: filter chain
		filter := httpFilterConfigFactory(config)(callbacks)
		return &bufferedFilter{
			callbacks: callbacks,
			filter:    filter,
		}
	}
}

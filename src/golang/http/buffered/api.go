package buffered

import "mosn.io/envoy-go-extension/http/api"

// request
type HttpDecoderFilter interface {
	// TODO: return status, for filter chain in go side
	Decode(api.RequestHeaderMap, api.BufferInstance, api.RequestTrailerMap)
}

// response
type StreamEncoderFilter interface {
	// TODO: return status, for filter chain in go side
	Encode(api.ResponseHeaderMap, api.BufferInstance, api.ResponseTrailerMap)
}

type HttpFilterConfigFactory func(config interface{}) HttpFilterFactory
type HttpFilterFactory func(callbacks api.FilterCallbackHandler) HttpFilter

type HttpFilter interface {
	HttpDecoderFilter
	StreamEncoderFilter
}

package main

import (
	udpa "github.com/cncf/xds/go/udpa/type/v1"
	"google.golang.org/protobuf/types/known/anypb"

	"mosn.io/envoy-go-extension/pkg/api"
	"mosn.io/envoy-go-extension/pkg/http"
)

func init() {
	http.RegisterHttpFilterConfigFactory(configFactory)
	http.RegisterHttpFilterConfigParser(&parser{})
}

type config struct {
	removeHeader string
	setHeader    string
}

type parser struct {
}

func (p *parser) Parse(any *anypb.Any) interface{} {
	configStruct := &udpa.TypedStruct{}
	any.UnmarshalTo(configStruct)
	v := configStruct.Value
	conf := &config{}
	if remove, ok := v.AsMap()["remove"].(string); ok {
		conf.removeHeader = remove
	}
	if set, ok := v.AsMap()["set"].(string); ok {
		conf.setHeader = set
	}
	return conf
}

func (p *parser) Merge(parent interface{}, child interface{}) interface{} {
	parentConfig := parent.(*config)
	childConfig := child.(*config)

	// copy one
	newConfig := *parentConfig
	if childConfig.removeHeader != "" {
		newConfig.removeHeader = childConfig.removeHeader
	}
	if childConfig.setHeader != "" {
		newConfig.setHeader = childConfig.setHeader
	}
	return &newConfig
}

type filter struct {
	config    *config
	callbacks api.FilterCallbackHandler
}

func (f *filter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	return api.Continue
}

func (f *filter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	return api.Continue
}

func (f *filter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	if f.config.removeHeader != "" {
		header.Remove(f.config.removeHeader)
	}
	if f.config.setHeader != "" {
		header.Set(f.config.setHeader, "test-value")
	}
	return api.Continue
}

func (f *filter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	return api.Continue
}

func (f *filter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	return api.Continue
}

func (f *filter) OnDestroy(reason api.DestroyReason) {
	// fmt.Printf("OnDestory, reason: %d\n", reason)
}

func configFactory(c interface{}) api.HttpFilterFactory {
	conf := c.(*config)
	return func(callbacks api.FilterCallbackHandler) api.HttpFilter {
		return &filter{
			config:    conf,
			callbacks: callbacks,
		}
	}
}

func main() {
}

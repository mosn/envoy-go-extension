package main

import (
	"fmt"
	udpa "github.com/cncf/xds/go/udpa/type/v1"
	"google.golang.org/protobuf/types/known/anypb"
	"google.golang.org/protobuf/types/known/structpb"
	"mosn.io/envoy-go-extension/http"
	"time"
)

type httpFilter struct {
	callbacks http.FilterCallbackHandler
	config    *structpb.Struct
}

func (f *httpFilter) DecodeHeaders(header http.RequestHeaderMap, endStream bool) http.StatusType {
	sleep := f.config.AsMap()["sleep"]
	if v, ok := sleep.(float64); ok {
		fmt.Printf("sleeping %v ms\n", v)
		time.Sleep(time.Millisecond * time.Duration(v))
	} else {
		fmt.Printf("config sleep is not number, %T\n", sleep)
	}
	fmt.Printf("header Get foo: %s, endStream: %v\n", header.Get("foo"), endStream)
	fmt.Printf("header GetRaw foo: %s, endStream: %v\n", header.GetRaw("foo"), endStream)
	return http.HeaderContinue
}

func (f *httpFilter) EncodeHeaders(header http.ResponseHeaderMap, endStream bool) http.StatusType {
	sleep := f.config.AsMap()["sleep"]
	if v, ok := sleep.(float64); ok {
		fmt.Printf("sleeping %v ms\n", v)
		time.Sleep(time.Millisecond * time.Duration(v))
	} else {
		fmt.Printf("config sleep is not number, %T\n", sleep)
	}
	fmt.Printf("header Get foo: %s, endStream: %v\n", header.Get("foo"), endStream)
	fmt.Printf("header GetRaw foo: %s, endStream: %v\n", header.GetRaw("foo"), endStream)
	return http.HeaderContinue
}

func (f *httpFilter) DecoderCallbacks() http.DecoderFilterCallbacks {
	return f.callbacks
}

func (f *httpFilter) EncoderCallbacks() http.EncoderFilterCallbacks {
	return f.callbacks
}

func registerHttpFilterConfigFactory(f http.HttpFilterConfigFactory) {
	httpFilterConfigFactory = f
}

func init() {
	registerHttpFilterConfigFactory(configFactory)
}

var httpFilterConfigFactory http.HttpFilterConfigFactory

func configFactory(any *anypb.Any) http.HttpFilterFactory {
	// TODO: unmarshal based on typeurl
	configStruct := &udpa.TypedStruct{}
	any.UnmarshalTo(configStruct)
	config := configStruct.Value

	return func(callbacks http.FilterCallbackHandler) http.HttpFilter {
		return &httpFilter{
			callbacks: callbacks,
			config:    config,
		}
	}
}

func getOrCreateHttpFilterFactory(configId uint64) http.HttpFilterFactory {
	config, ok := configCache[configId]
	if !ok {
		// TODO: panic
	}
	return httpFilterConfigFactory(config)
}

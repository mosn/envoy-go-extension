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
		fmt.Printf("decode headers, sleeping %v ms\n", v)
		time.Sleep(time.Millisecond * time.Duration(v))
	} else {
		fmt.Printf("config sleep is not number, %T\n", sleep)
	}
	fmt.Printf("request header Get foo: %s, endStream: %v\n", header.Get("foo"), endStream)
	fmt.Printf("request header GetRaw foo: %s, endStream: %v\n", header.GetRaw("foo"), endStream)
	return http.HeaderContinue
}

func (f *httpFilter) DecodeData(buffer http.BufferInstance, endStream bool) http.StatusType {
	sleep := f.config.AsMap()["sleep"]
	if v, ok := sleep.(float64); ok {
		fmt.Printf("decode data, sleeping %v ms\n", v)
		time.Sleep(time.Millisecond * time.Duration(v))
	} else {
		fmt.Printf("config sleep is not number, %T\n", sleep)
	}
	fmt.Printf("request data, length: %d, endStream: %v, data: %s\n", buffer.Length(), endStream, buffer.GetString())
	return http.DataContinue
}

func (f *httpFilter) EncodeHeaders(header http.ResponseHeaderMap, endStream bool) http.StatusType {
	sleep := f.config.AsMap()["sleep"]
	if v, ok := sleep.(float64); ok {
		fmt.Printf("encode headers, sleeping %v ms\n", v)
		time.Sleep(time.Millisecond * time.Duration(v))
	} else {
		fmt.Printf("config sleep is not number, %T\n", sleep)
	}
	fmt.Printf("response header Get date: %s, endStream: %v\n", header.Get("date"), endStream)
	fmt.Printf("response header GetRaw date: %s, endStream: %v\n", header.GetRaw("date"), endStream)

	header.Set("Foo", "Bar")
	// header.Set("content-length", "100")
	return http.HeaderContinue
}

func (f *httpFilter) EncodeData(buffer http.BufferInstance, endStream bool) http.StatusType {
	sleep := f.config.AsMap()["sleep"]
	if v, ok := sleep.(float64); ok {
		fmt.Printf("encode data, sleeping %v ms\n", v)
		time.Sleep(time.Millisecond * time.Duration(v))
	} else {
		fmt.Printf("config sleep is not number, %T\n", sleep)
	}
	fmt.Printf("response data, length: %d, endStream: %v, data: %s\n", buffer.Length(), endStream, buffer.GetString())
	return http.DataContinue
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

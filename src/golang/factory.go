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
	go func() {
		sleep := f.config.AsMap()["sleep"]
		if v, ok := sleep.(float64); ok {
			fmt.Printf("decode headers, sleeping %v ms\n", v)
			time.Sleep(time.Millisecond * time.Duration(v))
		} else {
			fmt.Printf("config sleep is not number, %T\n", sleep)
		}
		fmt.Printf("request header Get foo: %s, endStream: %v\n", header.Get("foo"), endStream)
		fmt.Printf("request header GetRaw foo: %s, endStream: %v\n", header.GetRaw("foo"), endStream)
		f.callbacks.Continue(http.Continue)
	}()
	return http.Running
}

func (f *httpFilter) DecodeData(buffer http.BufferInstance, endStream bool) http.StatusType {
	go func() {
		sleep := f.config.AsMap()["sleep"]
		if v, ok := sleep.(float64); ok {
			fmt.Printf("decode data, sleeping %v ms\n", v)
			time.Sleep(time.Millisecond * time.Duration(v))
		} else {
			fmt.Printf("config sleep is not number, %T\n", sleep)
		}
		fmt.Printf("request data, length: %d, endStream: %v\n", buffer.Length(), endStream)
		// fmt.Printf("request data: %s\n", buffer.GetString())
		f.callbacks.Continue(http.Continue)
	}()
	return http.Running
}

func (f *httpFilter) DecodeTrailers(trailers http.RequestTrailerMap) http.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 10)
		fmt.Printf("get request trailers, foo: %v\n", trailers.Get("foo"))
		f.callbacks.Continue(http.Continue)
	}()
	return http.Running
}

func (f *httpFilter) EncodeHeaders(header http.ResponseHeaderMap, endStream bool) http.StatusType {
	go func() {
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
		if !endStream {
			// header.Set("content-length", "3")
		}
		f.callbacks.Continue(http.Continue)
	}()
	return http.Running
}

func (f *httpFilter) EncodeData(buffer http.BufferInstance, endStream bool) http.StatusType {
	go func() {
		sleep := f.config.AsMap()["sleep"]
		if v, ok := sleep.(float64); ok {
			fmt.Printf("encode data, sleeping %v ms\n", v)
			time.Sleep(time.Millisecond * time.Duration(v))
		} else {
			fmt.Printf("config sleep is not number, %T\n", sleep)
		}
		fmt.Printf("response data, length: %d, endStream: %v, data: %s\n", buffer.Length(), endStream, buffer.GetString())
		// buffer.Set("foo=bar")
		f.callbacks.Continue(http.Continue)
	}()
	return http.Running
}

func (f *httpFilter) EncodeTrailers(trailers http.ResponseTrailerMap) http.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 10)
		fmt.Printf("get response trailers, AtEnd1: %v\n", trailers.Get("AtEnd1"))
		f.callbacks.Continue(http.Continue)
	}()
	return http.Running
}

func (f *httpFilter) OnDestroy(reason http.DestroyReason) {
	fmt.Printf("OnDestory, reason: %d\n", reason)
}

func (f *httpFilter) Callbacks() http.FilterCallbacks {
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

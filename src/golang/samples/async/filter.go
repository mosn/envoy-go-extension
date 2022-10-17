package async

import (
	"fmt"
	udpa "github.com/cncf/xds/go/udpa/type/v1"
	"google.golang.org/protobuf/types/known/anypb"
	"google.golang.org/protobuf/types/known/structpb"
	"mosn.io/envoy-go-extension/http/api"
	"time"
)

type httpFilter struct {
	callbacks api.FilterCallbackHandler
	config    *structpb.Struct
}

func (f *httpFilter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
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
		// f.callbacks.SendLocalReply(403, "forbidden from go", map[string]string{}, -1, "test-from-go")
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *httpFilter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
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
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *httpFilter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 10)
		fmt.Printf("get request trailers, foo: %v\n", trailers.Get("foo"))
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *httpFilter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
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
		f.callbacks.SendLocalReply(403, "forbidden from go", map[string]string{}, -1, "test-from-go")
		// f.callbacks.Continue(http.Continue)
	}()
	return api.Running
}

func (f *httpFilter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
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
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *httpFilter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 10)
		fmt.Printf("get response trailers, AtEnd1: %v\n", trailers.Get("AtEnd1"))
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *httpFilter) OnDestroy(reason api.DestroyReason) {
	fmt.Printf("OnDestory, reason: %d\n", reason)
}

func (f *httpFilter) Callbacks() api.FilterCallbacks {
	return f.callbacks
}

func ConfigFactory(config interface{}) api.HttpFilterFactory {
	// TODO: unmarshal based on typeurl
	any, ok := config.(*anypb.Any)
	if !ok {
		return nil
	}
	configStruct := &udpa.TypedStruct{}
	any.UnmarshalTo(configStruct)
	v := configStruct.Value

	return func(callbacks api.FilterCallbackHandler) api.HttpFilter {
		return &httpFilter{
			callbacks: callbacks,
			config:    v,
		}
	}
}

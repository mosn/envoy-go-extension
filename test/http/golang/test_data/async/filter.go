package main

import (
	"mosn.io/envoy-go-extension/pkg/http"
	"mosn.io/envoy-go-extension/pkg/http/api"
	"strconv"
	"time"
)

func init() {
	http.RegisterHttpFilterConfigFactory(configFactory)
}

type filter struct {
	callbacks        api.FilterCallbackHandler
	req_body_length  uint64
	resp_body_length uint64
}

func (f *filter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)

		header.Set("test_set_header_0", header.Get("x-test-header"))

		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *filter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)

		f.req_body_length += buffer.Length()
		buffer.Set(buffer.GetString())

		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *filter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *filter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)

		header.Set("test_set_header_0", header.Get("foo"))
		header.Set("test-req-body-length", strconv.Itoa(int(f.req_body_length)))

		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *filter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)

		f.resp_body_length += buffer.Length()

		if !endStream {
			buffer.Set("")
		} else {
			buffer.Set("origin-body-len: " + strconv.Itoa(int(f.resp_body_length)))
		}

		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *filter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
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

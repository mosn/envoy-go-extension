package main

import (
	"mosn.io/envoy-go-extension/pkg/http"
	"mosn.io/envoy-go-extension/pkg/http/api"
	"net/url"
	"strconv"
	"strings"
	"time"
)

func init() {
	http.RegisterHttpFilterConfigFactory(configFactory)
}

type filter struct {
	callbacks       api.FilterCallbackHandler
	req_body_length uint64
	query_params    url.Values
	path            string

	// test mode, from query parameters
	async      bool
	sleep      bool // all sleep
	data_sleep bool // only sleep in data phase
}

func parseQuery(path string) url.Values {
	if idx := strings.Index(path, "?"); idx >= 0 {
		query := path[idx+1:]
		values, _ := url.ParseQuery(query)
		return values
	}
	return make(url.Values)
}

func (f *filter) initRequest(header api.HeaderMap) {
	f.path = header.Get(":path")
	f.query_params = parseQuery(f.path)
	if f.query_params.Get("async") != "" {
		f.async = true
	}
	if f.query_params.Get("sleep") != "" {
		f.sleep = true
	}
	if f.query_params.Get("data_sleep") != "" {
		f.data_sleep = true
	}
}

// test: get, set, remove
func (f *filter) decodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	if f.sleep {
		time.Sleep(time.Millisecond * 100) // sleep 100 ms
	}
	header.Set("test-x-set-header-0", header.Get("x-test-header-0"))
	header.Remove("x-test-header-1")
	return api.Continue
}

// test: get, set
func (f *filter) decodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	if f.sleep || f.data_sleep {
		time.Sleep(time.Millisecond * 100) // sleep 100 ms
	}
	f.req_body_length += buffer.Length()
	data := buffer.GetString()
	buffer.Set(strings.ToUpper(data))
	return api.Continue
}

func (f *filter) decodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	if f.sleep {
		time.Sleep(time.Millisecond * 100) // sleep 100 ms
	}
	return api.Continue
}

func (f *filter) encodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	if f.sleep {
		time.Sleep(time.Millisecond * 100) // sleep 100 ms
	}
	header.Set("test-x-set-header-0", header.Get("x-test-header-0"))
	header.Remove("x-test-header-1")
	header.Set("test-req-body-length", strconv.Itoa(int(f.req_body_length)))
	header.Set("test-query-param-foo", f.query_params.Get("foo"))
	header.Set("test-path", f.path)
	return api.Continue
}

func (f *filter) encodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	if f.sleep || f.data_sleep {
		time.Sleep(time.Millisecond * 100) // sleep 100 ms
	}
	data := buffer.GetString()
	buffer.Set(strings.ToUpper(data))
	return api.Continue
}

func (f *filter) encodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	if f.sleep {
		time.Sleep(time.Millisecond * 100) // sleep 100 ms
	}
	return api.Continue
}

func (f *filter) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	f.initRequest(header)
	if f.async {
		go func() {
			status := f.decodeHeaders(header, endStream)
			f.callbacks.Continue(status)
		}()
		return api.Running
	} else {
		status := f.decodeHeaders(header, endStream)
		return status
	}
}

func (f *filter) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	if f.async {
		go func() {
			status := f.decodeData(buffer, endStream)
			f.callbacks.Continue(status)
		}()
		return api.Running
	} else {
		status := f.decodeData(buffer, endStream)
		return status
	}
}

func (f *filter) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	if f.async {
		go func() {
			status := f.decodeTrailers(trailers)
			f.callbacks.Continue(status)
		}()
		return api.Running
	} else {
		status := f.decodeTrailers(trailers)
		return status
	}
}

func (f *filter) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	if f.async {
		go func() {
			status := f.encodeHeaders(header, endStream)
			f.callbacks.Continue(status)
		}()
		return api.Running
	} else {
		status := f.encodeHeaders(header, endStream)
		return status
	}
}

func (f *filter) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	if f.async {
		go func() {
			status := f.encodeData(buffer, endStream)
			f.callbacks.Continue(status)
		}()
		return api.Running
	} else {
		status := f.encodeData(buffer, endStream)
		return status
	}
}

func (f *filter) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	if f.async {
		go func() {
			status := f.encodeTrailers(trailers)
			f.callbacks.Continue(status)
		}()
		return api.Running
	} else {
		status := f.encodeTrailers(trailers)
		return status
	}
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

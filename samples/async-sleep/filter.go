package async_sleep

import (
	"mosn.io/envoy-go-extension/pkg/http/api"
	"time"
)

// sleep 100 ms in each c => go call.

type asyncSleep struct {
	callbacks api.FilterCallbackHandler
}

func (f *asyncSleep) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) OnDestroy(reason api.DestroyReason) {
	// fmt.Printf("OnDestory, reason: %d\n", reason)
}

func (f *asyncSleep) Callbacks() api.FilterCallbacks {
	return f.callbacks
}

func ConfigFactory(interface{}) api.HttpFilterFactory {
	return func(callbacks api.FilterCallbackHandler) api.HttpFilter {
		return &asyncSleep{
			callbacks: callbacks,
		}
	}
}

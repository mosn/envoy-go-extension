package sofafun

import (
	"fmt"
	"mosn.io/envoy-go-extension/pkg/http/api"
	"mosn.io/envoy-go-extension/pkg/http/buffered"
	"strings"
)

type filter struct {
	config    interface{}
	callbacks api.FilterCallbackHandler
	skip      bool
}

func (f *filter) Decode(header api.RequestHeaderMap, data api.BufferInstance, trailer api.RequestTrailerMap) {
	path := header.Get(":path")
	fmt.Printf("path: %v\n", path)
	if path != "/" {
		f.skip = true
		// header.Remove("user-agent")
	} else {
		header.Remove("accept-encoding")
	}
}
func (f *filter) Encode(header api.ResponseHeaderMap, data api.BufferInstance, trailer api.ResponseTrailerMap) {
	if f.skip {
		return
	}
	contentType := header.Get("content-type")
	if !strings.Contains(contentType, "text/html") {
		return
	}

	str := `
<div style="
    position: fixed;
    width: 100%;
    top: 50%;
"> Mesage Mesage Mesage Mesage Mesage Mesage Mesage Mesage Mesage Mesage </div>
`
	if data != nil {
		body := data.GetString()
		fmt.Printf("orignal http body: %v\n", body)

		data.Set(body + str)
		header.Remove("content-length")
	}
	if trailer != nil {
		trailer.Set("t1", "v1")
	}
}

func ConfigFactory(config interface{}) buffered.HttpFilterFactory {
	return func(callbacks api.FilterCallbackHandler) buffered.HttpFilter {
		return &filter{
			config:    config,
			callbacks: callbacks,
		}
	}
}

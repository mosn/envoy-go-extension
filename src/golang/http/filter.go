/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package http

/*
// ref https://github.com/golang/go/issues/25832

#cgo linux LDFLAGS: -Wl,-unresolved-symbols=ignore-all
#cgo darwin LDFLAGS: -Wl,-undefined,dynamic_lookup

#include <stdlib.h>
#include <string.h>

#include "api.h"

*/
import "C"
import (
	"mosn.io/envoy-go-extension/http/api"
	"unsafe"
)

type httpRequest struct {
	req        *C.httpRequest
	httpFilter api.HttpFilter
}

func (r *httpRequest) Continue(status api.StatusType) {
	api := api.GetCgoAPI()
	api.HttpContinue(unsafe.Pointer(r.req), uint64(status))
}

func (r *httpRequest) SendLocalReply(response_code int, body_text string, headers map[string]string, grpc_status int64, details string) {
	api := api.GetCgoAPI()
	api.HttpSendLocalReply(unsafe.Pointer(r.req), response_code, body_text, headers, grpc_status, details)
}

func (r *httpRequest) Finalize(reason int) {
	api := api.GetCgoAPI()
	api.HttpFinalize(unsafe.Pointer(r.req), reason)
}

type httpHeader struct {
	request     *httpRequest
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

func (h *httpHeader) GetRaw(name string) string {
	api := api.GetCgoAPI()
	var value string
	api.HttpGetHeader(unsafe.Pointer(h.request.req), &name, &value)
	return value
}

func (h *httpHeader) Get(name string) string {
	api := api.GetCgoAPI()
	if h.headers == nil {
		h.headers = api.HttpCopyHeaders(unsafe.Pointer(h.request.req), h.headerNum, h.headerBytes)
	}
	if value, ok := h.headers[name]; ok {
		return value
	}
	return ""
}

func (h *httpHeader) Set(name, value string) {
	api := api.GetCgoAPI()
	if h.headers != nil {
		h.headers[name] = value
	}
	api.HttpSetHeader(unsafe.Pointer(h.request.req), &name, &value)
}

type httpBuffer struct {
	request   *httpRequest
	bufferPtr uint64
	length    uint64
	value     string
}

func (b *httpBuffer) GetString() string {
	if b.length == 0 {
		return ""
	}
	api := api.GetCgoAPI()
	api.HttpGetBuffer(unsafe.Pointer(b.request.req), b.bufferPtr, &b.value, b.length)
	return b.value
}

func (b *httpBuffer) Set(value string) {
	api := api.GetCgoAPI()
	api.HttpSetBuffer(unsafe.Pointer(b.request.req), b.bufferPtr, value)
}

func (b *httpBuffer) Length() uint64 {
	return b.length
}

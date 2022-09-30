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

package main

import (
	"mosn.io/envoy-go-extension/http"
)

type httpRequest struct {
	filter      uint64
	configId    uint64
	endStream   uint64
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

/*
func (r *httpRequest) ContinueDecoding(status http.StatusType) {
	api := http.GetCgoAPI()
	api.HttpDecodeContinue(r.filter, int(status))
}
*/

func (r *httpRequest) Continue(status http.StatusType) {
	api := http.GetCgoAPI()
	api.HttpContinue(r.filter, uint64(status))
}

type httpRequestHeader struct {
	request     *httpRequest
	endStream   uint64
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

func (h *httpRequestHeader) GetRaw(name string) string {
	api := http.GetCgoAPI()
	var value string
	api.HttpGetRequestHeader(h.request.filter, &name, &value)
	return value
}

func (h *httpRequestHeader) Get(name string) string {
	api := http.GetCgoAPI()
	if h.headers == nil {
		h.headers = api.HttpCopyRequestHeaders(h.request.filter, h.headerNum, h.headerBytes)
	}
	if value, ok := h.headers[name]; ok {
		return value
	}
	return ""
}

func (h *httpRequestHeader) Set(name, value string) {
	// TODO
}

type httpResponseHeader struct {
	request     *httpRequest
	endStream   uint64
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

func (h *httpResponseHeader) GetRaw(name string) string {
	api := http.GetCgoAPI()
	var value string
	api.HttpGetResponseHeader(h.request.filter, &name, &value)
	return value
}

func (h *httpResponseHeader) Get(name string) string {
	api := http.GetCgoAPI()
	if h.headers == nil {
		h.headers = api.HttpCopyResponseHeaders(h.request.filter, h.headerNum, h.headerBytes)
	}
	if value, ok := h.headers[name]; ok {
		return value
	}
	return ""
}

func (h *httpResponseHeader) Set(name, value string) {
	api := http.GetCgoAPI()
	api.HttpSetResponseHeader(h.request.filter, &name, &value)
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
	api := http.GetCgoAPI()
	api.HttpGetBuffer(b.request.filter, b.bufferPtr, &b.value, b.length)
	return b.value
}

func (b *httpBuffer) Set(value string) {
	api := http.GetCgoAPI()
	api.HttpSetBuffer(b.request.filter, b.bufferPtr, value)
}

func (b *httpBuffer) Length() uint64 {
	return b.length
}

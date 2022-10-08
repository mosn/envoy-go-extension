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
	filter     uint64
	httpFilter http.HttpFilter
}

func (r *httpRequest) Continue(status http.StatusType) {
	api := http.GetCgoAPI()
	api.HttpContinue(r.filter, uint64(status))
}

func (r *httpRequest) Finalize(reason int) {
	api := http.GetCgoAPI()
	api.HttpFinalize(r.filter, reason)
}

type httpHeader struct {
	request     *httpRequest
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

func (h *httpHeader) GetRaw(name string) string {
	api := http.GetCgoAPI()
	var value string
	api.HttpGetHeader(h.request.filter, &name, &value)
	return value
}

func (h *httpHeader) Get(name string) string {
	api := http.GetCgoAPI()
	if h.headers == nil {
		h.headers = api.HttpCopyHeaders(h.request.filter, h.headerNum, h.headerBytes)
	}
	if value, ok := h.headers[name]; ok {
		return value
	}
	return ""
}

func (h *httpHeader) Set(name, value string) {
	api := http.GetCgoAPI()
	if h.headers != nil {
		h.headers[name] = value
	}
	api.HttpSetHeader(h.request.filter, &name, &value)
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

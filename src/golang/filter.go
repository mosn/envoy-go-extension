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
	"fmt"
	"mosn.io/envoy-go-extension/http"
	"time"
)

var httpFilterFactory http.HttpFilterFactory

func registerHttpFilterFactory(f http.HttpFilterFactory) {
	httpFilterFactory = f
}

type httpCgoApiImpl struct{}

func (c *httpCgoApiImpl) HttpDecodeContinue(filter uint64, end int) {
	C.moeHttpDecodeContinue(C.ulonglong(filter), C.int(end))
}

func init() {
	http.SetCgoAPI(&httpCgoApiImpl{})
	registerHttpFilterFactory(factory)
}

type httpRequest struct {
	filter     uint64
	end_stream int
}

func (r *httpRequest) ContinueDecoding() {
	api := http.GetCgoAPI()
	api.HttpDecodeContinue(r.filter, r.end_stream)
}

func (r *httpRequest) Get(name string) string {
	return name
}

//export moeOnHttpDecodeHeader
func moeOnHttpDecodeHeader(filter uint64, end_stream int) {
	cb := &httpRequest{
		filter:     filter,
		end_stream: end_stream,
	}
	f := httpFilterFactory(cb)
	go func() {
		f.DecodeHeaders(cb, end_stream == 1)
		f.DecoderCallbacks().ContinueDecoding()
	}()
}

//export moeOnHttpDecodeData
func moeOnHttpDecodeData(filter uint64, end_stream int) {
	api := http.GetCgoAPI()
	api.HttpDecodeContinue(filter, end_stream)
}

type httpFilter struct {
	callbacks http.FilterCallbackHandler
}

func (f *httpFilter) DecodeHeaders(header http.RequestHeaderMap, end_stream bool) http.StatusType {
	fmt.Printf("header foo: %s, end_stream: %v\n", header.Get("foo"), end_stream)
	time.Sleep(time.Millisecond * 1000)
	return http.HeaderContinue
}

func (f *httpFilter) DecoderCallbacks() http.DecoderFilterCallbacks {
	return f.callbacks
}

func factory(callbacks http.FilterCallbackHandler) http.HttpFilter {
	return &httpFilter{
		callbacks: callbacks,
	}
}

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
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/anypb"
	"mosn.io/envoy-go-extension/http"
	"mosn.io/envoy-go-extension/utils"
	"reflect"
	"unsafe"
)

type httpCgoApiImpl struct{}

func (c *httpCgoApiImpl) HttpDecodeContinue(filter uint64, end int) {
	C.moeHttpDecodeContinue(C.ulonglong(filter), C.int(end))
}

func (c *httpCgoApiImpl) HttpGetRequestHeader(filter uint64, key *string, value *string) {
	C.moeHttpGetRequestHeader(C.ulonglong(filter), unsafe.Pointer(key), unsafe.Pointer(value))
}

func (c *httpCgoApiImpl) HttpCopyRequestHeaders(filter uint64, num uint64, bytes uint64) map[string]string {
	// TODO: use a memory pool for better performance,
	// but, should be very careful, since string is const in go,
	// and we have to make sure the strings is not using before reusing,
	// strings may be alive beyond the request life.
	strs := make([]string, num*2)
	buf := make([]byte, bytes)
	sHeader := (*reflect.SliceHeader)(unsafe.Pointer(&strs))
	bHeader := (*reflect.SliceHeader)(unsafe.Pointer(&buf))
	// sHeader.Len = int(num * 2)
	// bHeader.Len = int(bytes)

	C.moeHttpCopyRequestHeaders(C.ulonglong(filter), unsafe.Pointer(sHeader.Data), unsafe.Pointer(bHeader.Data))

	m := make(map[string]string, num)
	for i := uint64(0); i < num*2; i += 2 {
		key := strs[i]
		value := strs[i+1]
		m[key] = value

		// fmt.Printf("value of %s: %s\n", key, value)
	}
	return m
}

func init() {
	http.SetCgoAPI(&httpCgoApiImpl{})
}

type httpRequest struct {
	filter      uint64
	configId    uint64
	endStream   int
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

func (r *httpRequest) ContinueDecoding() {
	api := http.GetCgoAPI()
	api.HttpDecodeContinue(r.filter, r.endStream)
}

func (r *httpRequest) GetSingle(name string) string {
	api := http.GetCgoAPI()
	var value string
	api.HttpGetRequestHeader(r.filter, &name, &value)
	return value
}

func (r *httpRequest) Get(name string) string {
	api := http.GetCgoAPI()
	if r.headers == nil {
		r.headers = api.HttpCopyRequestHeaders(r.filter, r.headerNum, r.headerBytes)
	}
	if value, ok := r.headers[name]; ok {
		return value
	}
	return ""
}

var configId uint64
var configCache map[uint64]*anypb.Any = make(map[uint64]*anypb.Any, 16)

//export moeNewHttpPluginConfig
func moeNewHttpPluginConfig(configPtr uint64, configLen uint64) uint64 {
	buf := utils.BytesToSlice(configPtr, configLen)
	var any anypb.Any
	proto.Unmarshal(buf, &any)
	configId++
	configCache[configId] = &any
	return configId
}

//export moeDestoryHttpPluginConfig
func moeDestoryHttpPluginConfig(id uint64) {
	delete(configCache, id)
}

//export moeOnHttpDecodeHeader
func moeOnHttpDecodeHeader(filter uint64, configId uint64, endStream int, headerNum uint64, headerBytes uint64) {
	cb := &httpRequest{
		filter:      filter,
		configId:    configId,
		endStream:   endStream,
		headerNum:   headerNum,
		headerBytes: headerBytes,
	}
	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(cb)
	go func() {
		f.DecodeHeaders(cb, endStream == 1)
		f.DecoderCallbacks().ContinueDecoding()
	}()
}

//export moeOnHttpDecodeData
func moeOnHttpDecodeData(filter uint64, endStream int) {
	api := http.GetCgoAPI()
	api.HttpDecodeContinue(filter, endStream)
}

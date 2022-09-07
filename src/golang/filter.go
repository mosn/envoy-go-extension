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
)

type httpCgoApiImpl struct{}

func (c *httpCgoApiImpl) HttpDecodeContinue(filter uint64, end int) {
	C.moeHttpDecodeContinue(C.ulonglong(filter), C.int(end))
}

func init() {
	http.SetCgoAPI(&httpCgoApiImpl{})
}

type httpRequest struct {
	filter    uint64
	configId  uint64
	endStream int
}

func (r *httpRequest) ContinueDecoding() {
	api := http.GetCgoAPI()
	api.HttpDecodeContinue(r.filter, r.endStream)
}

func (r *httpRequest) Get(name string) string {
	return name
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
func moeOnHttpDecodeHeader(filter uint64, configId uint64, endStream int) {
	cb := &httpRequest{
		filter:    filter,
		configId:  configId,
		endStream: endStream,
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

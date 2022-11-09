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
	"errors"
	"runtime"
	"sync"
	"sync/atomic"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/anypb"
	"mosn.io/envoy-go-extension/pkg/http/api"
	"mosn.io/envoy-go-extension/pkg/utils"
)

var ErrDupRequestKey = errors.New("dup request key")

var Requests = &requestMap{}

type requestMap struct {
	m sync.Map // *C.httpRequest -> *httpRequest
}

func (f *requestMap) StoreReq(key *C.httpRequest, req *httpRequest) error {
	if _, loaded := f.m.LoadOrStore(key, req); loaded {
		return ErrDupRequestKey
	}
	return nil
}

func (f *requestMap) GetReq(key *C.httpRequest) *httpRequest {
	if v, ok := f.m.Load(key); ok {
		return v.(*httpRequest)
	}
	return nil
}

func (f *requestMap) DeleteReq(key *C.httpRequest) {
	f.m.Delete(key)
}

func (f *requestMap) Clear() {
	f.m.Range(func(key, _ interface{}) bool {
		f.m.Delete(key)
		return true
	})
}

func requestFinalize(r *httpRequest) {
	r.Finalize(api.NormalFinalize)
}

func createRequest(r *C.httpRequest) *httpRequest {
	req := &httpRequest{
		req: r,
	}
	// NP: make sure filter will be deleted.
	runtime.SetFinalizer(req, requestFinalize)

	// TODO: error
	_ = Requests.StoreReq(r, req)

	configId := uint64(r.configId)
	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(req)
	req.httpFilter = f

	return req
}

func getRequest(r *C.httpRequest) *httpRequest {
	return Requests.GetReq(r)
}

var (
	configNumGenerator uint64
	configCache        = &sync.Map{} // uint64 -> *anypb.Any
)

//export moeNewHttpPluginConfig
func moeNewHttpPluginConfig(configPtr uint64, configLen uint64) uint64 {
	buf := utils.BytesToSlice(configPtr, configLen)
	var any anypb.Any
	proto.Unmarshal(buf, &any)

	configNum := atomic.AddUint64(&configNumGenerator, 1)
	configCache.Store(configNum, &any)

	return configNum
}

//export moeDestoryHttpPluginConfig
func moeDestoryHttpPluginConfig(id uint64) {
	configCache.Delete(id)
}

//export moeOnHttpHeader
func moeOnHttpHeader(r *C.httpRequest, endStream, headerNum, headerBytes uint64) uint64 {
	var req *httpRequest
	phase := int(r.phase)
	if phase == api.DecodeHeaderPhase {
		req = createRequest(r)
	} else {
		req = getRequest(r)
		// early sendLocalReply may skip the whole decode phase
		if req == nil {
			req = createRequest(r)
		}
	}
	f := req.httpFilter

	header := &httpHeaderMap{
		request:     req,
		headerNum:   headerNum,
		headerBytes: headerBytes,
		isTrailer:   phase == api.DecodeTailerPhase || phase == api.EncodeTailerPhase,
	}

	var status api.StatusType
	switch phase {
	case api.DecodeHeaderPhase:
		status = f.DecodeHeaders(header, endStream == 1)
	case api.DecodeTailerPhase:
		status = f.DecodeTrailers(header)
	case api.EncodeHeaderPhase:
		status = f.EncodeHeaders(header, endStream == 1)
	case api.EncodeTailerPhase:
		status = f.EncodeTrailers(header)
	}
	return uint64(status)
}

//export moeOnHttpData
func moeOnHttpData(r *C.httpRequest, endStream, buffer, length uint64) uint64 {
	req := getRequest(r)

	f := req.httpFilter
	isDecode := int(r.phase) == api.DecodeDataPhase

	buf := &httpBuffer{
		request:   req,
		bufferPtr: buffer,
		length:    length,
	}
	/*
		id := ""
		if hf, ok := f.(*httpFilter); ok {
			id = hf.config.AsMap()["id"].(string)
		}
		fmt.Printf("id: %s, buffer ptr: %p, buffer data: %s\n", id, buffer, buf.Get())
	*/
	var status api.StatusType
	if isDecode {
		status = f.DecodeData(buf, endStream == 1)
	} else {
		status = f.EncodeData(buf, endStream == 1)
	}
	return uint64(status)
}

//export moeOnHttpDestroy
func moeOnHttpDestroy(r *C.httpRequest, reason uint64) {
	req := getRequest(r)

	v := api.DestroyReason(reason)

	f := req.httpFilter
	f.OnDestroy(v)

	Requests.DeleteReq(r)

	// no one is using req now, we can remove it manually, for better performance.
	if v == api.Normal {
		runtime.SetFinalizer(req, nil)
		req.Finalize(api.GCFinalize)
	}
}

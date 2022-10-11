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
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/anypb"
	"mosn.io/envoy-go-extension/http/api"
	"mosn.io/envoy-go-extension/utils"
	"reflect"
	"runtime"
	"unsafe"
)

type httpCgoApiImpl struct{}

func (c *httpCgoApiImpl) HttpContinue(r unsafe.Pointer, status uint64) {
	C.moeHttpContinue(r, C.int(status))
}

func (c *httpCgoApiImpl) HttpSendLocalReply(r unsafe.Pointer, response_code int, body_text string, headers map[string]string, grpc_status int64, details string) {
	hLen := len(headers)
	strs := make([]string, 0, hLen)
	for k, v := range headers {
		strs = append(strs, k, v)
	}
	C.moeHttpSendLocalReply(r, C.int(response_code), unsafe.Pointer(&body_text), unsafe.Pointer(&strs), C.longlong(grpc_status), unsafe.Pointer(&details))
}

func (c *httpCgoApiImpl) HttpGetHeader(r unsafe.Pointer, key *string, value *string) {
	C.moeHttpGetHeader(r, unsafe.Pointer(key), unsafe.Pointer(value))
}

func (c *httpCgoApiImpl) HttpCopyHeaders(r unsafe.Pointer, num uint64, bytes uint64) map[string]string {
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

	C.moeHttpCopyHeaders(r, unsafe.Pointer(sHeader.Data), unsafe.Pointer(bHeader.Data))

	m := make(map[string]string, num)
	for i := uint64(0); i < num*2; i += 2 {
		key := strs[i]
		value := strs[i+1]
		m[key] = value

		// fmt.Printf("value of %s: %s\n", key, value)
	}
	return m
}

func (c *httpCgoApiImpl) HttpSetHeader(r unsafe.Pointer, key *string, value *string) {
	C.moeHttpSetHeader(r, unsafe.Pointer(key), unsafe.Pointer(value))
}

func (c *httpCgoApiImpl) HttpGetBuffer(r unsafe.Pointer, bufferPtr uint64, value *string, length uint64) {
	buf := make([]byte, length)
	bHeader := (*reflect.SliceHeader)(unsafe.Pointer(&buf))
	sHeader := (*reflect.StringHeader)(unsafe.Pointer(value))
	sHeader.Data = bHeader.Data
	sHeader.Len = int(length)
	C.moeHttpGetBuffer(r, C.ulonglong(bufferPtr), unsafe.Pointer(bHeader.Data))
}

func (c *httpCgoApiImpl) HttpSetBuffer(r unsafe.Pointer, bufferPtr uint64, value string) {
	sHeader := (*reflect.StringHeader)(unsafe.Pointer(&value))
	C.moeHttpSetBuffer(r, C.ulonglong(bufferPtr), unsafe.Pointer(sHeader.Data), C.int(sHeader.Len))
}

func (c *httpCgoApiImpl) HttpFinalize(r unsafe.Pointer, reason int) {
	C.moeHttpFinalize(r, C.int(reason))
}

func init() {
	api.SetCgoAPI(&httpCgoApiImpl{})
}

var configNum uint64
var configCache = make(map[uint64]*anypb.Any, 16)

//export moeNewHttpPluginConfig
func moeNewHttpPluginConfig(configPtr uint64, configLen uint64) uint64 {
	buf := utils.BytesToSlice(configPtr, configLen)
	var any anypb.Any
	proto.Unmarshal(buf, &any)
	configNum++
	configCache[configNum] = &any
	return configNum
}

//export moeDestoryHttpPluginConfig
func moeDestoryHttpPluginConfig(id uint64) {
	delete(configCache, id)
}

var Requests = make(map[*C.httpRequest]*httpRequest, 64)

func requestFinalize(r *httpRequest) {
	r.Finalize(api.NormalFinalize)
}

func createRequest(r *C.httpRequest) *httpRequest {
	req := &httpRequest{
		req: r,
	}
	// NP: make sure filter will be deleted.
	runtime.SetFinalizer(req, requestFinalize)

	if _, ok := Requests[r]; ok {
		// TODO: error
	}
	Requests[r] = req

	configId := uint64(r.configId)
	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(req)
	req.httpFilter = f

	return req
}

func getRequest(r *C.httpRequest) *httpRequest {
	req, ok := Requests[r]
	if !ok {
		// TODO: error
	}
	return req
}

//export moeOnHttpHeader
func moeOnHttpHeader(r *C.httpRequest, endStream, headerNum, headerBytes uint64) uint64 {
	var req *httpRequest
	phase := int(r.phase)
	if phase == api.DecodeHeaderPhase {
		req = createRequest(r)
	} else {
		req = getRequest(r)
	}
	f := req.httpFilter

	header := &httpHeader{
		request:     req,
		headerNum:   headerNum,
		headerBytes: headerBytes,
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
	// f.Callbacks().Continue(status)
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
		fmt.Printf("id: %s, buffer ptr: %p, buffer data: %s\n", id, buffer, buf.GetString())
	*/
	var status api.StatusType
	if isDecode {
		status = f.DecodeData(buf, endStream == 1)
	} else {
		status = f.EncodeData(buf, endStream == 1)
	}
	// f.Callbacks().Continue(status)
	return uint64(status)
}

//export moeOnHttpDestroy
func moeOnHttpDestroy(r *C.httpRequest, reason uint64) {
	req := getRequest(r)

	v := api.DestroyReason(reason)

	f := req.httpFilter
	f.OnDestroy(v)

	Requests[r] = nil

	// no one is using req now, we can remove it manually, for better performance.
	if v == api.Normal {
		runtime.SetFinalizer(req, nil)
		req.Finalize(api.GCFinalize)
	}
}

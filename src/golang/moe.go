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
	"runtime"
	"unsafe"
)

type httpCgoApiImpl struct{}

func (c *httpCgoApiImpl) HttpContinue(filter uint64, status uint64) {
	C.moeHttpContinue(C.ulonglong(filter), C.int(status))
}

func (c *httpCgoApiImpl) HttpGetHeader(filter uint64, key *string, value *string) {
	C.moeHttpGetHeader(C.ulonglong(filter), unsafe.Pointer(key), unsafe.Pointer(value))
}

func (c *httpCgoApiImpl) HttpCopyHeaders(filter uint64, num uint64, bytes uint64) map[string]string {
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

	C.moeHttpCopyHeaders(C.ulonglong(filter), unsafe.Pointer(sHeader.Data), unsafe.Pointer(bHeader.Data))

	m := make(map[string]string, num)
	for i := uint64(0); i < num*2; i += 2 {
		key := strs[i]
		value := strs[i+1]
		m[key] = value

		// fmt.Printf("value of %s: %s\n", key, value)
	}
	return m
}

func (c *httpCgoApiImpl) HttpSetHeader(filter uint64, key *string, value *string) {
	C.moeHttpSetHeader(C.ulonglong(filter), unsafe.Pointer(key), unsafe.Pointer(value))
}

func (c *httpCgoApiImpl) HttpGetBuffer(filter uint64, bufferPtr uint64, value *string, length uint64) {
	buf := make([]byte, length)
	bHeader := (*reflect.SliceHeader)(unsafe.Pointer(&buf))
	sHeader := (*reflect.StringHeader)(unsafe.Pointer(value))
	sHeader.Data = bHeader.Data
	sHeader.Len = int(length)
	C.moeHttpGetBuffer(C.ulonglong(filter), C.ulonglong(bufferPtr), unsafe.Pointer(bHeader.Data))
}

func (c *httpCgoApiImpl) HttpSetBuffer(filter uint64, bufferPtr uint64, value string) {
	sHeader := (*reflect.StringHeader)(unsafe.Pointer(&value))
	C.moeHttpSetBuffer(C.ulonglong(filter), C.ulonglong(bufferPtr), unsafe.Pointer(sHeader.Data), C.int(sHeader.Len))
}

func (c *httpCgoApiImpl) HttpFinalize(filter uint64, reason int) {
	C.moeHttpFinalize(C.ulonglong(filter), C.int(reason))
}

func init() {
	http.SetCgoAPI(&httpCgoApiImpl{})
}

var configNum uint64
var configCache map[uint64]*anypb.Any = make(map[uint64]*anypb.Any, 16)

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

var Requests = make(map[uint64]*httpRequest, 64)

func requestFinalize(r *httpRequest) {
	r.Finalize(http.NormalFinalize)
}

func createRequest(filter, configId uint64) *httpRequest {
	req := &httpRequest{
		filter: filter,
	}
	// NP: make sure filter will be deleted.
	runtime.SetFinalizer(req, requestFinalize)

	if _, ok := Requests[filter]; ok {
		// TODO: error
	}
	Requests[filter] = req

	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(req)
	req.httpFilter = f

	return req
}

func getRequest(filter uint64) *httpRequest {
	req, ok := Requests[filter]
	if !ok {
		// TODO: error
	}
	return req
}

//export moeOnHttpHeader
func moeOnHttpHeader(filter, configId, endStream, isRequest, headerNum, headerBytes uint64) uint64 {
	var req *httpRequest
	if isRequest == 1 {
		req = createRequest(filter, configId)
	} else {
		req = getRequest(filter)
	}
	f := req.httpFilter

	header := &httpHeader{
		request:     req,
		headerNum:   headerNum,
		headerBytes: headerBytes,
	}

	var status http.StatusType
	if isRequest == 1 {
		status = f.DecodeHeaders(header, endStream == 1)
	} else {
		status = f.EncodeHeaders(header, endStream == 1)
	}
	// f.Callbacks().Continue(status)
	return uint64(status)
}

//export moeOnHttpData
func moeOnHttpData(filter, configId, endStream, isRequest, buffer, length uint64) uint64 {
	req := getRequest(filter)

	f := req.httpFilter

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
	var status http.StatusType
	if isRequest == 1 {
		status = f.DecodeData(buf, endStream == 1)
	} else {
		status = f.EncodeData(buf, endStream == 1)
	}
	// f.Callbacks().Continue(status)
	return uint64(status)
}

//export moeOnHttpDestroy
func moeOnHttpDestroy(filter, reason uint64) {
	req := getRequest(filter)

	r := http.DestroyReason(reason)

	f := req.httpFilter
	f.OnDestroy(r)

	Requests[filter] = nil

	// no one is using req now, we can remove it manually, for better performance.
	if r == http.Normal {
		runtime.SetFinalizer(req, nil)
		req.Finalize(http.GCFinalize)
	}
}

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
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/anypb"
	"mosn.io/envoy-go-extension/http"
	"mosn.io/envoy-go-extension/utils"
	"reflect"
	"unsafe"
)

type httpCgoApiImpl struct{}

func (c *httpCgoApiImpl) HttpContinue(filter uint64, status uint64) {
	C.moeHttpContinue(C.ulonglong(filter), C.int(status))
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

/* response */

/*
func (c *httpCgoApiImpl) HttpEncodeContinue(filter uint64, end int) {
	C.moeHttpEncodeContinue(C.ulonglong(filter), C.int(end))
}
*/

func (c *httpCgoApiImpl) HttpGetResponseHeader(filter uint64, key *string, value *string) {
	C.moeHttpGetResponseHeader(C.ulonglong(filter), unsafe.Pointer(key), unsafe.Pointer(value))
}

func (c *httpCgoApiImpl) HttpSetResponseHeader(filter uint64, key *string, value *string) {
	C.moeHttpSetResponseHeader(C.ulonglong(filter), unsafe.Pointer(key), unsafe.Pointer(value))
}

func (c *httpCgoApiImpl) HttpCopyResponseHeaders(filter uint64, num uint64, bytes uint64) map[string]string {
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

	C.moeHttpCopyResponseHeaders(C.ulonglong(filter), unsafe.Pointer(sHeader.Data), unsafe.Pointer(bHeader.Data))

	m := make(map[string]string, num)
	for i := uint64(0); i < num*2; i += 2 {
		key := strs[i]
		value := strs[i+1]
		m[key] = value

		// fmt.Printf("value of %s: %s\n", key, value)
	}
	return m
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

func init() {
	http.SetCgoAPI(&httpCgoApiImpl{})
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

var Requests map[*uint64]*httpRequest

//export moeOnHttpHeader
func moeOnHttpHeader(filter, configId, endStream, isRequest, headerNum, headerBytes uint64) uint64 {
	req := &httpRequest{
		filter: filter,
	}
	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(req)
	go func() {
		var status http.StatusType
		if isRequest == 1 {
			header := &httpRequestHeader{
				request:     req,
				endStream:   endStream,
				headerNum:   headerNum,
				headerBytes: headerBytes,
			}
			status = f.DecodeHeaders(header, endStream == 1)
		} else {
			header := &httpResponseHeader{
				request:     req,
				endStream:   endStream,
				headerNum:   headerNum,
				headerBytes: headerBytes,
			}
			status = f.EncodeHeaders(header, endStream == 1)
		}
		f.Callbacks().Continue(status)
	}()
	return uint64(http.Running)
}

//export moeOnHttpData
func moeOnHttpData(filter, configId, endStream, isRequest, buffer, length uint64) uint64 {
	req := &httpRequest{
		filter: filter,
	}
	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(req)
	buf := &httpBuffer{
		request:   req,
		bufferPtr: buffer,
		length:    length,
	}
	id := ""
	if hf, ok := f.(*httpFilter); ok {
		id = hf.config.AsMap()["id"].(string)
	}
	fmt.Printf("id: %s, buffer ptr: %p, buffer data: %s\n", id, buffer, buf.GetString())
	go func() {
		var status http.StatusType
		if isRequest == 1 {
			status = f.DecodeData(buf, endStream == 1)
		} else {
			status = f.EncodeData(buf, endStream == 1)
		}
		fmt.Printf("id: %s, data continue\n", id)
		f.Callbacks().Continue(status)
	}()
	return uint64(http.Running)
}

/*
//export moeOnHttpEncodeHeader
func moeOnHttpEncodeHeader(filter uint64, configId uint64, endStream int, headerNum uint64, headerBytes uint64) {
	req := &httpRequest{
		filter: filter,
	}
	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(req)
	go func() {
		header := &httpResponseHeader{
			request:     req,
			endStream:   endStream,
			headerNum:   headerNum,
			headerBytes: headerBytes,
		}
		f.EncodeHeaders(header, endStream == 1)
		f.EncoderCallbacks().ContinueEncoding()
	}()
}

//export moeOnHttpEncodeData
func moeOnHttpEncodeData(filter uint64, configId uint64, buffer uint64, length uint64, endStream int) {
	req := &httpRequest{
		filter: filter,
	}
	filterFactory := getOrCreateHttpFilterFactory(configId)
	f := filterFactory(req)
	go func() {
		buffer := &httpBuffer{
			request:   req,
			bufferPtr: buffer,
			length:    length,
		}
		f.EncodeData(buffer, endStream == 1)
		f.EncoderCallbacks().ContinueEncoding()
	}()
}
*/

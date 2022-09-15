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

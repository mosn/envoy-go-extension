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

import (
	"fmt"
	"strconv"
	"unsafe"

	"mosn.io/envoy-go-extension/pkg/api"
)

// api.HeaderMap
type headerMapImpl struct {
	request     *httpRequest
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

// ByteSize return size of HeaderMap
func (h *headerMapImpl) ByteSize() uint64 {
	return h.headerBytes
}

func (h *headerMapImpl) Range(f func(key, value string) bool) {
	panic("unsupported yet")
}

func (h *headerMapImpl) checkPhase(want api.EnvoyRequestPhase) {
	phase := api.EnvoyRequestPhase(h.request.req.phase)
	if phase != want {
		panic(fmt.Errorf("invalid phase %v while expect %v", phase, want))
	}

	if int(h.request.req.ingo) != api.IsInGo {
		// Already callback to Envoy, no Go API allowed.
		panic("unexpected API call")
	}
}

// api.RequestHeaderMap
type requestHeaderMapImpl struct {
	headerMapImpl
}

var _ api.RequestHeaderMap = (*requestHeaderMapImpl)(nil)

func (h *requestHeaderMapImpl) GetRaw(key string) string {
	h.checkPhase(api.DecodeHeaderPhase)
	var value string
	cAPI.HttpGetHeader(unsafe.Pointer(h.request.req), &key, &value)
	return value
}

func (h *requestHeaderMapImpl) Get(key string) (string, bool) {
	h.checkPhase(api.DecodeHeaderPhase)
	if h.headers == nil {
		h.headers = cAPI.HttpCopyHeaders(unsafe.Pointer(h.request.req), h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	return value, ok
}

func (h *requestHeaderMapImpl) Set(key, value string) {
	h.checkPhase(api.DecodeHeaderPhase)
	if h.headers != nil {
		h.headers[key] = value
	}
	cAPI.HttpSetHeader(unsafe.Pointer(h.request.req), &key, &value)
}

func (h *requestHeaderMapImpl) Add(key, value string) {
	h.checkPhase(api.DecodeHeaderPhase)
	// TODO: add
}

func (h *requestHeaderMapImpl) Del(key string) {
	h.checkPhase(api.DecodeHeaderPhase)
	if h.headers != nil {
		delete(h.headers, key)
	}
	cAPI.HttpRemoveHeader(unsafe.Pointer(h.request.req), &key)
}

func (h *requestHeaderMapImpl) Protocol() string {
	v, _ := h.Get(":protocol")
	return v
}

func (h *requestHeaderMapImpl) Scheme() string {
	v, _ := h.Get(":scheme")
	return v
}

func (h *requestHeaderMapImpl) Method() string {
	v, _ := h.Get(":method")
	return v
}

func (h *requestHeaderMapImpl) Path() string {
	v, _ := h.Get(":path")
	return v
}

func (h *requestHeaderMapImpl) Host() string {
	v, _ := h.Get(":authority")
	return v
}

// api.ResponseHeaderMap
type responseHeaderMapImpl struct {
	headerMapImpl
}

var _ api.ResponseHeaderMap = (*responseHeaderMapImpl)(nil)

func (h *responseHeaderMapImpl) Get(key string) (string, bool) {
	h.checkPhase(api.EncodeHeaderPhase)
	if h.headers == nil {
		h.headers = cAPI.HttpCopyHeaders(unsafe.Pointer(h.request.req), h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	return value, ok
}

func (h *responseHeaderMapImpl) Set(key, value string) {
	h.checkPhase(api.EncodeHeaderPhase)
	if h.headers != nil {
		h.headers[key] = value
	}
	cAPI.HttpSetHeader(unsafe.Pointer(h.request.req), &key, &value)
}

func (h *responseHeaderMapImpl) Add(key, value string) {
	h.checkPhase(api.EncodeHeaderPhase)
	// TODO: add
}

func (h *responseHeaderMapImpl) Del(key string) {
	h.checkPhase(api.EncodeHeaderPhase)
	if h.headers != nil {
		delete(h.headers, key)
	}
	cAPI.HttpRemoveHeader(unsafe.Pointer(h.request.req), &key)
}

func (h *responseHeaderMapImpl) Status() int {
	if str, ok := h.Get(":status"); ok {
		v, _ := strconv.Atoi(str)
		return v
	}
	return 0
}

// api.RequestTrailerMap
type requestTrailerMapImpl struct {
	headerMapImpl
}

var _ api.RequestTrailerMap = (*requestTrailerMapImpl)(nil)

func (h *requestTrailerMapImpl) Get(key string) (string, bool) {
	h.checkPhase(api.DecodeTrailerPhase)
	if h.headers == nil {
		h.headers = cAPI.HttpCopyTrailers(unsafe.Pointer(h.request.req), h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	return value, ok
}

func (h *requestTrailerMapImpl) Set(key, value string) {
	h.checkPhase(api.DecodeTrailerPhase)
	if h.headers != nil {
		h.headers[key] = value
	}
	cAPI.HttpSetTrailer(unsafe.Pointer(h.request.req), &key, &value)
}

func (h *requestTrailerMapImpl) Add(key, value string) {
	h.checkPhase(api.DecodeTrailerPhase)
	// TODO: add
}

func (h *requestTrailerMapImpl) Del(key string) {
	h.checkPhase(api.DecodeTrailerPhase)
	panic("implement me")
}

// api.ResponseTrailerMap
type responseTrailerMapImpl struct {
	headerMapImpl
}

var _ api.ResponseTrailerMap = (*responseTrailerMapImpl)(nil)

func (h *responseTrailerMapImpl) Get(key string) (string, bool) {
	h.checkPhase(api.EncodeTrailerPhase)
	if h.headers == nil {
		h.headers = cAPI.HttpCopyTrailers(unsafe.Pointer(h.request.req), h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	return value, ok
}

func (h *responseTrailerMapImpl) Set(key, value string) {
	h.checkPhase(api.EncodeTrailerPhase)
	if h.headers != nil {
		h.headers[key] = value
	}
	cAPI.HttpSetTrailer(unsafe.Pointer(h.request.req), &key, &value)
}

func (h *responseTrailerMapImpl) Add(key, value string) {
	h.checkPhase(api.EncodeTrailerPhase)
	// TODO: add
}

func (h *responseTrailerMapImpl) Del(key string) {
	h.checkPhase(api.EncodeTrailerPhase)
	panic("implement me")
}

// api.BufferInstance
type httpBuffer struct {
	request             *httpRequest
	envoyBufferInstance uint64
	length              uint64
	value               string
}

var _ api.BufferInstance = (*httpBuffer)(nil)

func (b *httpBuffer) Write(p []byte) (n int, err error) {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, string(p), api.AppendBuffer)
	return len(p), nil
}

func (b *httpBuffer) WriteString(s string) (n int, err error) {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, s, api.AppendBuffer)
	return len(s), nil
}

func (b *httpBuffer) WriteByte(p byte) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, string(p), api.AppendBuffer)
	return nil
}

func (b *httpBuffer) WriteUint16(p uint16) error {
	s := strconv.FormatUint(uint64(p), 10)
	_, err := b.WriteString(s)
	return err
}

func (b *httpBuffer) WriteUint32(p uint32) error {
	s := strconv.FormatUint(uint64(p), 10)
	_, err := b.WriteString(s)
	return err
}

func (b *httpBuffer) WriteUint64(p uint64) error {
	s := strconv.FormatUint(uint64(p), 10)
	_, err := b.WriteString(s)
	return err
}

func (b *httpBuffer) Peek(n int) []byte {
	panic("implement me")
}

func (b *httpBuffer) Bytes() []byte {
	if b.length == 0 {
		return nil
	}
	cAPI.HttpGetBuffer(unsafe.Pointer(b.request.req), b.envoyBufferInstance, &b.value, b.length)
	return []byte(b.value)
}

func (b *httpBuffer) Drain(offset int) {
	panic("implement me")
}

func (b *httpBuffer) Len() int {
	return int(b.length)
}

func (b *httpBuffer) Reset() {
	panic("implement me")
}

func (b *httpBuffer) String() string {
	if b.length == 0 {
		return ""
	}
	cAPI.HttpGetBuffer(unsafe.Pointer(b.request.req), b.envoyBufferInstance, &b.value, b.length)
	return b.value
}

func (b *httpBuffer) Append(data []byte) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, string(data), api.AppendBuffer)
	return nil
}

func (b *httpBuffer) Prepend(data []byte) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, string(data), api.PrependBuffer)
	return nil
}

func (b *httpBuffer) AppendString(s string) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, s, api.AppendBuffer)
	return nil
}

func (b *httpBuffer) PrependString(s string) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, s, api.PrependBuffer)
	return nil
}

func (b *httpBuffer) Set(data []byte) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, string(data), api.SetBuffer)
	return nil
}

func (b *httpBuffer) SetString(s string) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, s, api.SetBuffer)
	return nil
}

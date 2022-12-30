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
	"strconv"

	"mosn.io/envoy-go-extension/pkg/api"
)

// panic error messages when C API return not ok
var (
	ErrRequestFinished = "request has been finished"
	ErrFilterDestroyed = "golang filter has been destroyed"
	ErrNotInGo         = "not proccessing Go"
	ErrInvalidPhase    = "invalid phase, maybe headers/buffer already continued"
)

// api.HeaderMap
type headerMapImpl struct {
	request     *httpRequest
	headers     map[string][]string
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

func (h *headerMapImpl) Get(key string) (string, bool) {
	if h.headers == nil {
		h.headers = cAPI.HttpCopyHeaders(h.request, h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	if !ok {
		return "", false
	}
	return value[0], ok
}

func (h *headerMapImpl) Values(key string) []string {
	if h.headers == nil {
		h.headers = cAPI.HttpCopyHeaders(h.request, h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	if !ok {
		return nil
	}
	return value
}

func (h *headerMapImpl) Set(key, value string) {
	if h.headers != nil {
		h.headers[key] = []string{value}
	}
	cAPI.HttpSetHeader(h.request, &key, &value, false)
}

func (h *headerMapImpl) Add(key, value string) {
	if h.headers != nil {
		if hdrs, found := h.headers[key]; found {
			h.headers[key] = append(hdrs, value)
		} else {
			h.headers[key] = []string{value}
		}
	}
	cAPI.HttpSetHeader(h.request, &key, &value, true)
}

func (h *headerMapImpl) Del(key string) {
	if h.headers != nil {
		delete(h.headers, key)
	}
	cAPI.HttpRemoveHeader(h.request, &key)
}

// api.RequestHeaderMap
type requestHeaderMapImpl struct {
	headerMapImpl
}

var _ api.RequestHeaderMap = (*requestHeaderMapImpl)(nil)

func (h *requestHeaderMapImpl) GetRaw(key string) string {
	var value string
	cAPI.HttpGetHeader(h.request, &key, &value)
	return value
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

func (h *responseHeaderMapImpl) GetRaw(key string) string {
	var value string
	cAPI.HttpGetHeader(h.request, &key, &value)
	return value
}

func (h *responseHeaderMapImpl) Del(key string) {
	if h.headers != nil {
		delete(h.headers, key)
	}
	cAPI.HttpRemoveHeader(h.request, &key)
}

func (h *responseHeaderMapImpl) Status() int {
	if str, ok := h.Get(":status"); ok {
		v, _ := strconv.Atoi(str)
		return v
	}
	return 0
}

// api.HeaderMap
type headerTrailerMapImpl struct {
	request     *httpRequest
	headers     map[string][]string
	headerNum   uint64
	headerBytes uint64
}

// ByteSize return size of HeaderMap
func (h *headerTrailerMapImpl) ByteSize() uint64 {
	return h.headerBytes
}

func (h *headerTrailerMapImpl) Range(f func(key, value string) bool) {
	panic("unsupported yet")
}

func (h *headerTrailerMapImpl) Get(key string) (string, bool) {
	if h.headers == nil {
		h.headers = cAPI.HttpCopyTrailers(h.request, h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	if !ok {
		return "", false
	}
	return value[0], ok
}

func (h *headerTrailerMapImpl) Values(key string) []string {
	if h.headers == nil {
		h.headers = cAPI.HttpCopyTrailers(h.request, h.headerNum, h.headerBytes)
	}
	value, ok := h.headers[key]
	if !ok {
		return nil
	}
	return value
}

func (h *headerTrailerMapImpl) Set(key, value string) {
	if h.headers != nil {
		h.headers[key] = []string{value}
	}
	cAPI.HttpSetTrailer(h.request, &key, &value)
}

func (h *headerTrailerMapImpl) Add(key, value string) {
	// TODO: add
}

func (h *headerTrailerMapImpl) Del(key string) {
	panic("todo")
}

// api.RequestTrailerMap
type requestTrailerMapImpl struct {
	headerTrailerMapImpl
}

var _ api.RequestTrailerMap = (*requestTrailerMapImpl)(nil)

// api.ResponseTrailerMap
type responseTrailerMapImpl struct {
	headerTrailerMapImpl
}

var _ api.ResponseTrailerMap = (*responseTrailerMapImpl)(nil)

// api.BufferInstance
type httpBuffer struct {
	request             *httpRequest
	envoyBufferInstance uint64
	length              uint64
	value               string
}

var _ api.BufferInstance = (*httpBuffer)(nil)

func (b *httpBuffer) Write(p []byte) (n int, err error) {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, string(p), api.AppendBuffer)
	return len(p), nil
}

func (b *httpBuffer) WriteString(s string) (n int, err error) {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, s, api.AppendBuffer)
	return len(s), nil
}

func (b *httpBuffer) WriteByte(p byte) error {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, string(p), api.AppendBuffer)
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
	cAPI.HttpGetBuffer(b.request, b.envoyBufferInstance, &b.value, b.length)
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
	cAPI.HttpGetBuffer(b.request, b.envoyBufferInstance, &b.value, b.length)
	return b.value
}

func (b *httpBuffer) Append(data []byte) error {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, string(data), api.AppendBuffer)
	return nil
}

func (b *httpBuffer) Prepend(data []byte) error {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, string(data), api.PrependBuffer)
	return nil
}

func (b *httpBuffer) AppendString(s string) error {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, s, api.AppendBuffer)
	return nil
}

func (b *httpBuffer) PrependString(s string) error {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, s, api.PrependBuffer)
	return nil
}

func (b *httpBuffer) Set(data []byte) error {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, string(data), api.SetBuffer)
	return nil
}

func (b *httpBuffer) SetString(s string) error {
	cAPI.HttpSetBufferHelper(b.request, b.envoyBufferInstance, s, api.SetBuffer)
	return nil
}

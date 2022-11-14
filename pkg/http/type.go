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
	"io"
	"strconv"
	"unsafe"

	mosnApi "mosn.io/api"
	"mosn.io/pkg/buffer"

	"mosn.io/envoy-go-extension/pkg/http/api"
)

type httpHeaderMap struct {
	request     *httpRequest
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
	isTrailer   bool
}

var _ api.RequestHeaderMap = (*httpHeaderMap)(nil)
var _ api.RequestTrailerMap = (*httpHeaderMap)(nil)
var _ api.ResponseHeaderMap = (*httpHeaderMap)(nil)
var _ api.ResponseTrailerMap = (*httpHeaderMap)(nil)

func (h *httpHeaderMap) GetRaw(key string) string {
	if h.isTrailer {
		panic("unsupported yet")
	}
	var value string
	cAPI.HttpGetHeader(unsafe.Pointer(h.request.req), &key, &value)
	return value
}

func (h *httpHeaderMap) Get(key string) (string, bool) {
	if h.headers == nil {
		if h.isTrailer {
			h.headers = cAPI.HttpCopyTrailers(unsafe.Pointer(h.request.req), h.headerNum, h.headerBytes)
		} else {
			h.headers = cAPI.HttpCopyHeaders(unsafe.Pointer(h.request.req), h.headerNum, h.headerBytes)
		}
	}
	value, ok := h.headers[key]
	return value, ok
}

func (h *httpHeaderMap) Set(key, value string) {
	if h.headers != nil {
		h.headers[key] = value
	}
	if h.isTrailer {
		cAPI.HttpSetTrailer(unsafe.Pointer(h.request.req), &key, &value)
	} else {
		cAPI.HttpSetHeader(unsafe.Pointer(h.request.req), &key, &value)
	}
}

func (h *httpHeaderMap) Add(key, value string) {
	// TODO: add
}

func (h *httpHeaderMap) Del(key string) {
	if h.headers != nil {
		delete(h.headers, key)
	}
	if h.isTrailer {
		panic("unsupported yet")
	} else {
		cAPI.HttpRemoveHeader(unsafe.Pointer(h.request.req), &key)
	}
}

func (h *httpHeaderMap) Range(f func(key, value string) bool) {
	panic("unsupported yet")
}

// ByteSize return size of HeaderMap
func (h *httpHeaderMap) ByteSize() uint64 {
	return h.headerBytes
}

type httpBuffer struct {
	buffer.IoBuffer
	request             *httpRequest
	envoyBufferInstance uint64
	length              uint64
	value               string
}

var _ api.BufferInstance = (*httpBuffer)(nil)

func (b *httpBuffer) Read(p []byte) (n int, err error) {
	panic("implement me")
}

func (b *httpBuffer) ReadOnce(r io.Reader) (n int64, err error) {
	panic("implement me")
}

func (b *httpBuffer) ReadFrom(r io.Reader) (n int64, err error) {
	panic("implement me")
}

func (b *httpBuffer) Grow(n int) error {
	panic("implement me")
}

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

func (b *httpBuffer) WriteTo(w io.Writer) (n int64, err error) {
	panic("implement me")
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

func (b *httpBuffer) Cap() int {
	panic("implement me")
}

func (b *httpBuffer) Reset() {
	panic("implement me")
}

func (b *httpBuffer) Clone() mosnApi.IoBuffer {
	panic("implement me")
}

func (b *httpBuffer) String() string {
	if b.length == 0 {
		return ""
	}
	cAPI.HttpGetBuffer(unsafe.Pointer(b.request.req), b.envoyBufferInstance, &b.value, b.length)
	return b.value
}

func (b *httpBuffer) Alloc(i int) {
	panic("implement me")
}

func (b *httpBuffer) Free() {
	panic("implement me")
}

func (b *httpBuffer) Count(i int32) int32 {
	panic("implement me")
}

func (b *httpBuffer) EOF() bool {
	panic("implement me")
}

func (b *httpBuffer) SetEOF(eof bool) {
	panic("implement me")
}

func (b *httpBuffer) Append(data []byte) error {
	cAPI.HttpSetBufferHelper(unsafe.Pointer(b.request.req), b.envoyBufferInstance, string(data), api.AppendBuffer)
	return nil
}

func (b *httpBuffer) CloseWithError(err error) {
	panic("implement me")
}

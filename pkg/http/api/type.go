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

package api

import (
	mosnApi "mosn.io/api"
	"mosn.io/pkg/buffer"
)

// ****************** filter status start ******************//
type StatusType int

const (
	Running                StatusType = 0
	LocalReply             StatusType = 1
	Continue               StatusType = 2
	StopAndBuffer          StatusType = 3
	StopAndBufferWatermark StatusType = 4
	StopNoBuffer           StatusType = 5
)

// header status
// refer https://github.com/envoyproxy/envoy/blob/main/envoy/http/filter.h
const (
	HeaderContinue                     StatusType = 100
	HeaderStopIteration                StatusType = 101
	HeaderContinueAndDontEndStream     StatusType = 102
	HeaderStopAllIterationAndBuffer    StatusType = 103
	HeaderStopAllIterationAndWatermark StatusType = 104
)

// data status
// refer https://github.com/envoyproxy/envoy/blob/main/envoy/http/filter.h
const (
	DataContinue                  StatusType = 200
	DataStopIterationAndBuffer    StatusType = 201
	DataStopIterationAndWatermark StatusType = 202
	DataStopIterationNoBuffer     StatusType = 203
)

// Trailer status
// refer https://github.com/envoyproxy/envoy/blob/main/envoy/http/filter.h
const (
	TrailerContinue      StatusType = 300
	TrailerStopIteration StatusType = 301
)

//****************** filter status end ******************//

// ****************** log level start ******************//
type LogType int

// refer https://github.com/envoyproxy/envoy/blob/main/source/common/common/base_logger.h
const (
	Trace    LogType = 0
	Debug    LogType = 1
	Info     LogType = 2
	Warn     LogType = 3
	Error    LogType = 4
	Critical LogType = 5
)

//******************* log level end *******************//

// ****************** HeaderMap start ******************//

type RequestHeaderMap interface {
	mosnApi.HeaderMap
	// GetRaw is unsafe, reuse the memory from Envoy
	GetRaw(name string) string
}

type RequestTrailerMap interface {
	mosnApi.HeaderMap
	// GetRaw is unsafe, reuse the memory from Envoy
	GetRaw(name string) string
}

type ResponseHeaderMap interface {
	mosnApi.HeaderMap
	// GetRaw is unsafe, reuse the memory from Envoy
	GetRaw(name string) string
}

type ResponseTrailerMap interface {
	mosnApi.HeaderMap
	// GetRaw is unsafe, reuse the memory from Envoy
	GetRaw(name string) string
}

type MetadataMap interface {
}

//****************** HeaderMap end ******************//

// *************** BufferInstance start **************//
type BufferAction int

const (
	SetBuffer     BufferAction = 0
	AppendBuffer  BufferAction = 1
	PrependBuffer BufferAction = 2
)

type BufferInstance interface {
	buffer.IoBuffer
}

//*************** BufferInstance end **************//

type DestroyReason int

const (
	Normal    DestroyReason = 0
	Terminate DestroyReason = 1
)

const (
	NormalFinalize int = 0 // normal, finalize on destroy
	GCFinalize     int = 1 // finalize in GC sweep
)

type EnvoyRequestPhase int

const (
	DecodeHeaderPhase EnvoyRequestPhase = iota + 1
	DecodeDataPhase
	DecodeTrailerPhase
	EncodeHeaderPhase
	EncodeDataPhase
	EncodeTrailerPhase
)

func (e EnvoyRequestPhase) String() string {
	switch e {
	case DecodeHeaderPhase:
		return "DecodeHeader"
	case DecodeDataPhase:
		return "DecodeData"
	case DecodeTrailerPhase:
		return "DecodeTrailer"
	case EncodeHeaderPhase:
		return "EncodeHeader"
	case EncodeDataPhase:
		return "EncodeData"
	case EncodeTrailerPhase:
		return "EncodeTrailer"
	}
	return "unknown phase"
}
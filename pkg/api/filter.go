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

import "google.golang.org/protobuf/types/known/anypb"

// request
type HttpDecoderFilter interface {
	DecodeHeaders(RequestHeaderMap, bool) StatusType
	DecodeData(BufferInstance, bool) StatusType
	DecodeTrailers(RequestTrailerMap) StatusType
	/*
		DecodeMetadata(MetadataMap) StatusType
	*/
}

type HttpFilterConfigParser interface {
	Parse(any *anypb.Any) interface{}
	Merge(parentConfig interface{}, childConfig interface{}) interface{}
}

type HttpFilterConfigFactory func(config interface{}) HttpFilterFactory
type HttpFilterFactory func(callbacks FilterCallbackHandler) HttpFilter

type HttpFilter interface {
	// http request
	HttpDecoderFilter
	// response stream
	StreamEncoderFilter
	/*
		// stream complete
		OnStreamComplete()
		// error log
		Log(LogType, string)
	*/
	// destroy filter
	OnDestroy(DestroyReason)
}

// response
type StreamEncoderFilter interface {
	EncodeHeaders(ResponseHeaderMap, bool) StatusType
	EncodeData(BufferInstance, bool) StatusType
	EncodeTrailers(ResponseTrailerMap) StatusType
	/*
		EncodeMetadata(MetadataMap) StatusType
		EncoderCallbacks() EncoderFilterCallbacks
	*/
}

type DynamicMetadata interface {
	Get(filterName string) map[string]interface{}
	Set(filterName string, key string, value interface{})
}

// stream info
// refer https://github.com/envoyproxy/envoy/blob/main/envoy/stream_info/stream_info.h
type StreamInfo interface {
	GetRouteName() string
	DynamicMetadata() DynamicMetadata
	/*
		VirtualClusterName() string
		BytesReceived() int64
		BytesSent() int64
		Protocol() string
		ResponseCode() int
		GetRequestHeaders() RequestHeaderMap
		ResponseCodeDetails() string
	*/
}

type StreamFilterCallbacks interface {
	StreamInfo() StreamInfo
}

type FilterCallbacks interface {
	StreamFilterCallbacks
	// Continue or SendLocalReply should be last API invoked, no more code after them.
	Continue(StatusType)
	SendLocalReply(responseCode int, bodyText string, headers map[string]string, grpcStatus int64, details string)
	// RecoverPanic recover panic in defer
	RecoverPanic()
	/*
		AddDecodedData(buffer BufferInstance, streamingFilter bool)
	*/
}

type FilterCallbackHandler interface {
	FilterCallbacks
}

/* cluster specifier plugin */

type ClusterConfigParser interface {
	Parse(any *anypb.Any) interface{}
}

type ClusterSpecifier interface {
	// TODO: support header
	// Choose(RequestHeaderMap) string
	Choose() string
}

type ClusterSpecifierFactory func(config interface{}) ClusterSpecifier

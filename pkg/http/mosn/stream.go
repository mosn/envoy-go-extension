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

package mosn

import (
	"context"
	"runtime"

	udpa "github.com/cncf/xds/go/udpa/type/v1"
	"google.golang.org/protobuf/types/known/anypb"
	mosnApi "mosn.io/api"
	"mosn.io/mosn/pkg/streamfilter"
	mosnSync "mosn.io/mosn/pkg/sync"
	"mosn.io/pkg/buffer"
	"mosn.io/pkg/log"
	"mosn.io/pkg/variable"

	"mosn.io/envoy-go-extension/pkg/http/api"
)

var workerPool mosnSync.WorkerPool

func init() {
	poolSize := runtime.NumCPU() * 256
	workerPool = mosnSync.NewWorkerPool(poolSize)
}

func ConfigFactory(config interface{}) api.HttpFilterFactory {
	any, ok := config.(*anypb.Any)
	if !ok {
		return nil
	}
	configStruct := &udpa.TypedStruct{}
	if err := any.UnmarshalTo(configStruct); err != nil {
		log.DefaultLogger.Errorf("[moe] ConfigFactory fail to unmarshal, err: %v", err)
		return nil
	}
	v, ok := configStruct.Value.AsMap()["filter_chain"]
	if !ok {
		log.DefaultLogger.Errorf("[moe] ConfigFactory fail to get filter_chain")
		return nil
	}
	filterChainName := v.(string)

	return func(callbacks api.FilterCallbackHandler) api.HttpFilter {
		ctx := buffer.NewBufferPoolContext(context.Background())
		ctx = variable.NewVariableContext(ctx)

		buf := moeBuffersByContext(ctx)
		if buf.stream == nil {
			buf.stream = &ActiveStream{
				ctx:             ctx,
				filterChainName: filterChainName,
				filterChain:     CreateStreamFilterChain(ctx, filterChainName),
				workPool:        workerPool,
				callbacks:       callbacks,
			}
		}

		buf.stream.filterChain.SetReceiveFilterHandler(buf.stream)
		buf.stream.filterChain.SetSenderFilterHandler(buf.stream)

		return buf.stream
	}
}

type ActiveStream struct {
	ctx                 context.Context
	filterChainName     string
	filterChain         *streamfilter.DefaultStreamFilterChainImpl
	currentReceivePhase mosnApi.ReceiverFilterPhase
	callbacks           api.FilterCallbackHandler
	reqHeader           mosnApi.HeaderMap
	reqBody             mosnApi.IoBuffer
	reqTrailer          mosnApi.HeaderMap
	respHeader          mosnApi.HeaderMap
	respBody            mosnApi.IoBuffer
	respTrailer         mosnApi.HeaderMap
	workPool            mosnSync.WorkerPool
}

var _ api.HttpFilter = (*ActiveStream)(nil)
var _ mosnApi.StreamReceiverFilterHandler = (*ActiveStream)(nil)
var _ mosnApi.StreamSenderFilterHandler = (*ActiveStream)(nil)

func (s *ActiveStream) SetCurrentReveivePhase(phase mosnApi.ReceiverFilterPhase) {
	s.currentReceivePhase = phase
}

func (s *ActiveStream) runReceiverFilters() {
	s.workPool.ScheduleAuto(func() {
		s.SetCurrentReveivePhase(mosnApi.BeforeRoute)
		s.filterChain.RunReceiverFilter(s.ctx, mosnApi.BeforeRoute, s.reqHeader, s.reqBody, s.reqTrailer, nil)
		s.callbacks.Continue(api.Continue)
	})
}

func (s *ActiveStream) runSenderFilters() {
	s.workPool.ScheduleAuto(func() {
		s.filterChain.RunSenderFilter(s.ctx, mosnApi.BeforeSend, s.respHeader, s.respBody, s.respTrailer, nil)
		s.callbacks.Continue(api.Continue)
	})
}

func (s *ActiveStream) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	s.reqHeader = &headerMapImpl{header}
	if endStream {
		s.runReceiverFilters()
	}
	return api.Running
}

func (s *ActiveStream) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	s.reqBody = &bufferImpl{buffer}
	if endStream {
		s.runReceiverFilters()
	}
	return api.Running
}

func (s *ActiveStream) DecodeTrailers(trailer api.RequestTrailerMap) api.StatusType {
	s.reqTrailer = &headerMapImpl{trailer}
	s.runReceiverFilters()
	return api.Running
}

func (s *ActiveStream) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	s.respHeader = &headerMapImpl{header}
	if endStream {
		s.runSenderFilters()
	}
	return api.Running
}

func (s *ActiveStream) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	s.respBody = &bufferImpl{buffer}
	if endStream {
		s.runSenderFilters()
	}
	return api.Running
}

func (s *ActiveStream) EncodeTrailers(trailer api.ResponseTrailerMap) api.StatusType {
	s.respTrailer = &headerMapImpl{trailer}
	s.runSenderFilters()
	return api.Running
}

func (s *ActiveStream) OnDestroy(reason api.DestroyReason) {
	s.filterChain.OnDestroy()
	DestroyStreamFilterChain(s.filterChain)

	if ctx := buffer.PoolContext(s.ctx); ctx != nil {
		ctx.Give()
	}
}

func (s *ActiveStream) AppendHeaders(headers mosnApi.HeaderMap, endStream bool) {
	panic("implement me")
}

func (s *ActiveStream) AppendData(buf mosnApi.IoBuffer, endStream bool) {
	panic("implement me")
}

func (s *ActiveStream) AppendTrailers(trailers mosnApi.HeaderMap) {
	panic("implement me")
}

func (s *ActiveStream) SendHijackReply(code int, headers mosnApi.HeaderMap) {
	panic("implement me")
}

func (s *ActiveStream) SendHijackReplyWithBody(code int, headers mosnApi.HeaderMap, body string) {
	panic("implement me")
}

func (s *ActiveStream) SendDirectResponse(headers mosnApi.HeaderMap, buf mosnApi.IoBuffer, trailers mosnApi.HeaderMap) {
	panic("implement me")
}

func (s *ActiveStream) TerminateStream(code int) bool {
	panic("implement me")
}

func (s *ActiveStream) GetRequestHeaders() mosnApi.HeaderMap {
	return s.reqHeader
}

func (s *ActiveStream) SetRequestHeaders(headers mosnApi.HeaderMap) {
	panic("implement me")
}

func (s *ActiveStream) GetRequestData() mosnApi.IoBuffer {
	return s.reqBody
}

func (s *ActiveStream) SetRequestData(buf mosnApi.IoBuffer) {
	panic("implement me")
}

func (s *ActiveStream) GetRequestTrailers() mosnApi.HeaderMap {
	return s.reqTrailer
}

func (s *ActiveStream) SetRequestTrailers(trailers mosnApi.HeaderMap) {
	panic("implement me")
}

func (s *ActiveStream) GetFilterCurrentPhase() mosnApi.ReceiverFilterPhase {
	return s.currentReceivePhase
}

func (s *ActiveStream) Route() mosnApi.Route {
	return nil
}

func (s *ActiveStream) RequestInfo() mosnApi.RequestInfo {
	panic("implement me")
}

func (s *ActiveStream) Connection() mosnApi.Connection {
	panic("implement me")
}

func (s *ActiveStream) GetResponseHeaders() mosnApi.HeaderMap {
	return s.respHeader
}

func (s *ActiveStream) SetResponseHeaders(headers mosnApi.HeaderMap) {
	panic("implement me")
}

func (s *ActiveStream) GetResponseData() mosnApi.IoBuffer {
	return s.respBody
}

func (s *ActiveStream) SetResponseData(buf mosnApi.IoBuffer) {
	panic("implement me")
}

func (s *ActiveStream) GetResponseTrailers() mosnApi.HeaderMap {
	return s.respTrailer
}

func (s *ActiveStream) SetResponseTrailers(trailers mosnApi.HeaderMap) {
	panic("implement me")
}
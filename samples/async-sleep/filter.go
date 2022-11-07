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

package async_sleep

import (
	"time"

	"mosn.io/envoy-go-extension/pkg/http/api"
)

// sleep 100 ms in each c => go call.

type asyncSleep struct {
	callbacks api.FilterCallbackHandler
}

func (f *asyncSleep) DecodeHeaders(header api.RequestHeaderMap, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) DecodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) DecodeTrailers(trailers api.RequestTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) EncodeHeaders(header api.ResponseHeaderMap, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) EncodeData(buffer api.BufferInstance, endStream bool) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) EncodeTrailers(trailers api.ResponseTrailerMap) api.StatusType {
	go func() {
		time.Sleep(time.Millisecond * 100)
		f.callbacks.Continue(api.Continue)
	}()
	return api.Running
}

func (f *asyncSleep) OnDestroy(reason api.DestroyReason) {
	// fmt.Printf("OnDestory, reason: %d\n", reason)
}

func ConfigFactory(interface{}) api.HttpFilterFactory {
	return func(callbacks api.FilterCallbackHandler) api.HttpFilter {
		return &asyncSleep{
			callbacks: callbacks,
		}
	}
}

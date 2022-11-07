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

package buffered

import (
	"mosn.io/envoy-go-extension/pkg/http/api"
	"mosn.io/envoy-go-extension/pkg/http/buffered"
)

type filter struct {
	config    interface{}
	callbacks api.FilterCallbackHandler
}

func (f *filter) Decode(header api.RequestHeaderMap, data api.BufferInstance, trailer api.RequestTrailerMap) {
}
func (f *filter) Encode(header api.ResponseHeaderMap, data api.BufferInstance, trailer api.ResponseTrailerMap) {
	if data != nil {
		data.Set("Foo-Bar")
		header.Set("Content-Length", "7")
	}
	header.Set("foo", "bar")
	if trailer != nil {
		trailer.Set("t1", "v1")
	}
}

func ConfigFactory(config interface{}) buffered.HttpFilterFactory {
	return func(callbacks api.FilterCallbackHandler) buffered.HttpFilter {
		return &filter{
			config:    config,
			callbacks: callbacks,
		}
	}
}

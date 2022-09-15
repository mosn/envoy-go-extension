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

package main

import (
	"mosn.io/envoy-go-extension/http"
)

type httpRequest struct {
	filter      uint64
	configId    uint64
	endStream   int
	headers     map[string]string
	headerNum   uint64
	headerBytes uint64
}

func (r *httpRequest) ContinueDecoding() {
	api := http.GetCgoAPI()
	api.HttpDecodeContinue(r.filter, r.endStream)
}

func (r *httpRequest) GetRaw(name string) string {
	api := http.GetCgoAPI()
	var value string
	api.HttpGetRequestHeader(r.filter, &name, &value)
	return value
}

func (r *httpRequest) Get(name string) string {
	api := http.GetCgoAPI()
	if r.headers == nil {
		r.headers = api.HttpCopyRequestHeaders(r.filter, r.headerNum, r.headerBytes)
	}
	if value, ok := r.headers[name]; ok {
		return value
	}
	return ""
}

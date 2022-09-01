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

/*
// ref https://github.com/golang/go/issues/25832

#cgo linux LDFLAGS: -Wl,-unresolved-symbols=ignore-all
#cgo darwin LDFLAGS: -Wl,-undefined,dynamic_lookup

#include <stdlib.h>
#include <string.h>

#include "api.h"

*/
import "C"

import "mosn.io/envoy-go-extension/http"

type httpCgoApiImpl struct{}

func (c *httpCgoApiImpl) HttpRequestContinue(filter uint64, end int) {
	C.moeHttpRequestContinue(C.ulonglong(filter), C.int(end))
}

func init() {
	http.SetCgoAPI(&httpCgoApiImpl{})
}

//export moeOnHttpRequestHeader
func moeOnHttpRequestHeader(filter uint64, end_stream int) {
	api := http.GetCgoAPI()
	api.HttpRequestContinue(filter, end_stream)
}

//export moeOnHttpRequestData
func moeOnHttpRequestData(filter uint64, end_stream int) {
	api := http.GetCgoAPI()
	api.HttpRequestContinue(filter, end_stream)
}

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

#cgo CFLAGS: -Ipkg/api
#cgo linux LDFLAGS: -Wl,-unresolved-symbols=ignore-all
#cgo darwin LDFLAGS: -Wl,-undefined,dynamic_lookup

#include <stdlib.h>
#include <string.h>

#include "api.h"

*/
import "C"

import (
	_ "mosn.io/envoy-go-extension/pkg/api"
	_ "mosn.io/envoy-go-extension/pkg/http"
)

//go:linkname moeNewHttpPluginConfig mosn.io/envoy-go-extension/pkg/http.moeNewHttpPluginConfig
//export moeNewHttpPluginConfig
func moeNewHttpPluginConfig(configPtr uint64, configLen uint64) uint64

//go:linkname moeDestroyHttpPluginConfig mosn.io/envoy-go-extension/pkg/http.moeDestroyHttpPluginConfig
//export moeDestroyHttpPluginConfig
func moeDestroyHttpPluginConfig(id uint64)

//go:linkname moeMergeHttpPluginConfig mosn.io/envoy-go-extension/pkg/http.moeMergeHttpPluginConfig
//export moeMergeHttpPluginConfig
func moeMergeHttpPluginConfig(parentId uint64, childId uint64) uint64

//go:linkname moeOnHttpHeader mosn.io/envoy-go-extension/pkg/http.moeOnHttpHeader
//export moeOnHttpHeader
func moeOnHttpHeader(r *C.httpRequest, endStream, headerNum, headerBytes uint64) uint64

//go:linkname moeOnHttpData mosn.io/envoy-go-extension/pkg/http.moeOnHttpData
//export moeOnHttpData
func moeOnHttpData(r *C.httpRequest, endStream, buffer, length uint64) uint64

//go:linkname moeOnHttpDestroy mosn.io/envoy-go-extension/pkg/http.moeOnHttpDestroy
//export moeOnHttpDestroy
func moeOnHttpDestroy(r *C.httpRequest, reason uint64)

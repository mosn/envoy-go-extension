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

/*
// ref https://github.com/golang/go/issues/25832

#cgo linux LDFLAGS: -Wl,-unresolved-symbols=ignore-all
#cgo darwin LDFLAGS: -Wl,-undefined,dynamic_lookup

#include <stdlib.h>
#include <string.h>

#include "api.h"

*/
import "C"
import (
	"fmt"
	"unsafe"

	"mosn.io/envoy-go-extension/pkg/api"
)

// api.FilterCallbacks
type httpRequest struct {
	req        *C.httpRequest
	httpFilter api.HttpFilter
}

func (r *httpRequest) checkState() {
	if int(r.req.ingo) != api.IsInGo {
		// Already callback to Envoy, no Go API allowed.
		panic("unexpected API call")
	}
}

func (r *httpRequest) Continue(status api.StatusType) {
	if status == api.LocalReply {
		fmt.Printf("warning: LocalReply status is useless after sendLocalReply, ignoring")
		return
	}
	r.checkState()
	cAPI.HttpContinue(unsafe.Pointer(r.req), uint64(status))
}

func (r *httpRequest) SendLocalReply(responseCode int, bodyText string, headers map[string]string, grpcStatus int64, details string) {
	r.checkState()
	cAPI.HttpSendLocalReply(unsafe.Pointer(r.req), responseCode, bodyText, headers, grpcStatus, details)
}

func (r *httpRequest) StreamInfo() api.StreamInfo {
	return &streamInfo{
		request: r,
	}
}

func (r *httpRequest) Finalize(reason int) {
	cAPI.HttpFinalize(unsafe.Pointer(r.req), reason)
}

// api.StreamInfo
type streamInfo struct {
	request *httpRequest
}

func (s *streamInfo) GetRouteName() string {
	s.request.checkState()
	return cAPI.HttpGetRouteName(unsafe.Pointer(s.request.req))
}

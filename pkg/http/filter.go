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

#cgo CFLAGS: -I../api
#cgo linux LDFLAGS: -Wl,-unresolved-symbols=ignore-all
#cgo darwin LDFLAGS: -Wl,-undefined,dynamic_lookup

#include <stdlib.h>
#include <string.h>

#include "api.h"

*/
import "C"
import (
	"fmt"
	"runtime/debug"
	"sync"

	"mosn.io/envoy-go-extension/pkg/api"
)

type httpRequest struct {
	req        *C.httpRequest
	httpFilter api.HttpFilter
	paniced    bool
	safePanic  bool

	// lock the req_->strValue in the C side, which do not allow concurrency.
	mutex sync.Mutex

	// when Go try to read data from Go thread, not in the envoy worker thread,
	// C will post a callback to Envoy worker thread,
	// then, we use this sema to wait the callback from the Envoy worker thread.
	sema sync.WaitGroup
}

func (r *httpRequest) Phase() string {
	return api.EnvoyRequestPhase(r.req.phase).String()
}

func (r *httpRequest) Continue(status api.StatusType) {
	if status == api.LocalReply {
		fmt.Printf("warning: LocalReply status is useless after sendLocalReply, ignoring")
		return
	}
	cAPI.HttpContinue(r, uint64(status))
}

func (r *httpRequest) SendLocalReply(responseCode int, bodyText string, headers map[string]string, grpcStatus int64, details string) {
	cAPI.HttpSendLocalReply(r, responseCode, bodyText, headers, grpcStatus, details)
}

func (r *httpRequest) safeReplyPanic() {
	r.safePanic = true
	defer r.RecoverPanic()
	r.SendLocalReply(500, "error happened in Go filter\r\n", map[string]string{}, 0, "")
	r.safePanic = false
}

func (r *httpRequest) RecoverPanic() {
	if e := recover(); e != nil {
		// TODO: support register logger?
		fmt.Printf("got panic: %v, phase: %v, stack: %s\n", e, r.Phase(), debug.Stack())
		switch e {
		case ErrRequestFinished, ErrFilterDestroyed:
			// do nothing

		case ErrNotInGo:
			// we can not send local reply now, since not in go.
			r.paniced = true

		default:
			// ErrInvalidPhase, or panic from other places, not from not-ok C return status.
			// It's safe to try send a local reply with 500 status.
			if r.safePanic {
				// sendLocalReply(means safe mode) may only may get ErrRequestFinished, ErrFilterDestroyed or ErrNotInGo,
				// won't get these panic errors
				// TODO: race: two goroutines panics in the same time?
				panic("impossible")
			}
			r.safeReplyPanic()
		}
	}
}

func (r *httpRequest) StreamInfo() api.StreamInfo {
	return &streamInfo{
		request: r,
	}
}

func (r *httpRequest) Finalize(reason int) {
	cAPI.HttpFinalize(r, reason)
}

type streamInfo struct {
	request *httpRequest
}

func (s *streamInfo) GetRouteName() string {
	return cAPI.HttpGetRouteName(s.request)
}

type dynamicMetadata struct {
	request *httpRequest
}

func (s *streamInfo) DynamicMetadata() api.DynamicMetadata {
	return &dynamicMetadata{
		request: s.request,
	}
}

func (d *dynamicMetadata) Get(filterName string) map[string]interface{} {
	return cAPI.HttpGetDynamicMetadata(d.request, filterName)
}

func (d *dynamicMetadata) Set(filterName string, key string, value interface{}) {
	cAPI.HttpSetDynamicMetadata(d.request, filterName, key, value)
}

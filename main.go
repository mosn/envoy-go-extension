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
	"mosn.io/envoy-go-extension/pkg/http"
	"mosn.io/envoy-go-extension/pkg/http/buffered"
	"mosn.io/envoy-go-extension/samples/async"
	asyncSleep "mosn.io/envoy-go-extension/samples/async-sleep"
	bufferedSample "mosn.io/envoy-go-extension/samples/buffered"
)

func init() {
	// these are useless samples
	buffered.RegisterHttpFilterConfigFactory(bufferedSample.ConfigFactory)
	http.RegisterHttpFilterConfigFactory(asyncSleep.ConfigFactory)
	http.RegisterHttpFilterConfigFactory(async.ConfigFactory)
	buffered.RegisterHttpFilterConfigFactory(bufferedSample.ConfigFactory)

	// default filter
	http.RegisterHttpFilterConfigFactory(http.PassThroughFactory)
}

func main() {
}

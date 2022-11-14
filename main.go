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
	"os"

	_ "mosn.io/mosn/pkg/filter/stream/dsl"
	_ "mosn.io/mosn/pkg/filter/stream/dubbo"
	_ "mosn.io/mosn/pkg/filter/stream/faultinject"
	_ "mosn.io/mosn/pkg/filter/stream/faulttolerance"
	_ "mosn.io/mosn/pkg/filter/stream/flowcontrol"
	_ "mosn.io/mosn/pkg/filter/stream/grpcmetric"
	_ "mosn.io/mosn/pkg/filter/stream/gzip"
	_ "mosn.io/mosn/pkg/filter/stream/headertometadata"
	_ "mosn.io/mosn/pkg/filter/stream/ipaccess"
	_ "mosn.io/mosn/pkg/filter/stream/mirror"
	_ "mosn.io/mosn/pkg/filter/stream/payloadlimit"
	_ "mosn.io/mosn/pkg/filter/stream/proxywasm"
	_ "mosn.io/mosn/pkg/filter/stream/seata"
	_ "mosn.io/mosn/pkg/filter/stream/transcoder/http2bolt"
	_ "mosn.io/mosn/pkg/filter/stream/transcoder/httpconv"
	"mosn.io/mosn/pkg/streamfilter"

	_ "mosn.io/envoy-go-extension/pkg/filter/stream/echo"
	"mosn.io/envoy-go-extension/pkg/http"
)

var DefaultMosnConfigPath string = "/home/admin/mosn/config/mosn.json"

func init() {
	http.RegisterHttpFilterConfigFactory(http.ConfigFactory)

	// load mosn config
	mosnConfigPath := DefaultMosnConfigPath
	if envPath := os.Getenv("MOSN_CONFIG"); envPath != "" {
		mosnConfigPath = envPath
	}
	streamfilter.LoadAndRegisterStreamFilters(mosnConfigPath)
}

func main() {
}

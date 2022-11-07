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

import (
	"mosn.io/envoy-go-extension/pkg/http/api"
)

// pass through by default
var httpFilterConfigFactory api.HttpFilterConfigFactory = PassThroughFactory

// raw mode, sync by default
func RegisterHttpFilterConfigFactory(f api.HttpFilterConfigFactory) {
	httpFilterConfigFactory = f
}

func getOrCreateHttpFilterFactory(configId uint64) api.HttpFilterFactory {
	config, ok := configCache[configId]
	if !ok {
		// TODO: panic
	}
	return httpFilterConfigFactory(config)
}

// streaming and async supported by default
func RegisterStreamingHttpFilterConfigFactory(f api.HttpFilterConfigFactory) {
	httpFilterConfigFactory = f
}

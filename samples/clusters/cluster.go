package clusters

import "mosn.io/envoy-go-extension/pkg/api"

type specifier struct {
	config interface{}
}

func ClusterSpecifierFactory(config interface{}) api.ClusterSpecifier {
	return &specifier{config: config}
}

func (s *specifier) Choose() string {
	// use the default cluster
	return ""
}

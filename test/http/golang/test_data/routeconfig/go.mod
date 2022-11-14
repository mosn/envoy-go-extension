module example.com/routeconfig

go 1.18

require (
	github.com/cncf/xds/go v0.0.0-20220520190051-1e77728a1eaa
	mosn.io/envoy-go-extension v0.0.0-20221020015753-d29836e4c75f
)

require (
	github.com/golang/protobuf v1.5.0 // indirect
	google.golang.org/protobuf v1.28.1
)

replace mosn.io/envoy-go-extension => ../../../../../

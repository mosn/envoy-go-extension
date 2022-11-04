module example.com/passthrough

go 1.18

require mosn.io/envoy-go-extension v0.0.0-20221020091338-0a61e17d6514

require google.golang.org/protobuf v1.28.1 // indirect

replace mosn.io/envoy-go-extension => ../../../../../

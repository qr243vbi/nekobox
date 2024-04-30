module grpc_server

go 1.19

require (
	github.com/grpc-ecosystem/go-grpc-middleware v1.4.0
	github.com/matsuridayo/libneko v0.0.0-20230913024055-5277a5bfc889
	google.golang.org/grpc v1.63.2
	google.golang.org/protobuf v1.33.0
)

require (
	golang.org/x/net v0.22.0 // indirect
	golang.org/x/sys v0.18.0 // indirect
	golang.org/x/text v0.14.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240314234333-6e1732d8331c // indirect
)

replace cloud.google.com/go => cloud.google.com/go v0.112.2

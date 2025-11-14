find_package(Qt6 COMPONENTS Core Grpc Protobuf REQUIRED)

# Generate protobuf message classes
qt_add_protobuf(MyProtoTarget
    PROTO_FILES core/server/gen/libcore.proto
)

# Generate gRPC client classes
qt_add_grpc(MyGrpcTarget CLIENT
    PROTO_FILES core/server/gen/libcore.proto
)

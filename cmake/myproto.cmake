find_package(Qt6 REQUIRED COMPONENTS Core Grpc) 

# Define your .proto files
set(PROTO_FILES
    core/server/gen/libcore.proto
)


add_library(myproto ${PROTO_FILES})
# Generate Protobuf and gRPC code
qt_add_protobuf(myproto PROTO_FILES ${PROTO_FILES})
qt_add_grpc(myproto CLIENT PROTO_FILES ${PROTO_FILES})

target_link_libraries(myproto
    PRIVATE
        Qt6::Core
        Qt6::Grpc
)
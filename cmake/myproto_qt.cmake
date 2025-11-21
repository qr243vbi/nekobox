# Find the required Qt components, including Protobuf
find_package(Qt6 REQUIRED COMPONENTS Protobuf Grpc)

# Define your .proto files
set(PROTO_FILES
    core/server/gen/libcore.proto
)

# You can use a single target for both generated outputs
add_library(myproto STATIC ${PROTO_FILES} )

# Generate Protobuf and gRPC code and add the sources to the 'myproto' target

qt_add_grpc(myproto CLIENT PROTO_FILES ${PROTO_FILES})

qt_add_protobuf(myproto PROTO_FILES ${PROTO_FILES})

target_link_libraries(myproto
    PRIVATE
        Qt6::Core
        Qt6::Protobuf # Link against the Qt Protobuf library
        Qt6::Grpc     # Link against the Qt Grpc library
)

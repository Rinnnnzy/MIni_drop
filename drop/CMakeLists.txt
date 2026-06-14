cmake_minimum_required(VERSION 3.16)
project(mini_drop CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)

set(PROTO_DIR ${CMAKE_SOURCE_DIR}/common/proto)

foreach(PROTO common healthcheck hotmethod control init)
  add_custom_command(
    OUTPUT
      ${CMAKE_BINARY_DIR}/${PROTO}.pb.cc ${CMAKE_BINARY_DIR}/${PROTO}.pb.h
      ${CMAKE_BINARY_DIR}/${PROTO}.grpc.pb.cc ${CMAKE_BINARY_DIR}/${PROTO}.grpc.pb.h
    COMMAND protoc
    ARGS --grpc_out=${CMAKE_BINARY_DIR} --cpp_out=${CMAKE_BINARY_DIR}
         -I${PROTO_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${PROTO_DIR}/${PROTO}.proto
    DEPENDS ${PROTO_DIR}/${PROTO}.proto
  )
  list(APPEND ALL_PROTO_SRCS
    ${CMAKE_BINARY_DIR}/${PROTO}.pb.cc
    ${CMAKE_BINARY_DIR}/${PROTO}.grpc.pb.cc)
endforeach()

file(GLOB COMMON_SRCS common/*.cpp)
add_library(drop_common ${COMMON_SRCS} ${ALL_PROTO_SRCS})
target_include_directories(drop_common PUBLIC ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(drop_common PUBLIC gRPC::grpc++ protobuf::libprotobuf Threads::Threads)

file(GLOB SERVER_SRCS server/*.cpp)
add_executable(drop_server ${SERVER_SRCS})
target_link_libraries(drop_server PRIVATE drop_common)

file(GLOB AGENT_SRCS agent/*.cpp)
add_executable(drop_agent ${AGENT_SRCS})
target_link_libraries(drop_agent PRIVATE drop_common)

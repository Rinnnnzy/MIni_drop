cmake_minimum_required(VERSION 3.16)
project(mini_drop CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# apt 版 libgrpc++-dev / libprotobuf-dev 不提供 CMake config 文件
# （find_package(gRPC CONFIG) 依赖库作者打包的 .cmake 文件，apt 包只给了 pkg-config 的 .pc 文件），
# 所以改用 pkg-config 方式查找，这是 apt 安装版 gRPC 唯一可靠的查找方式。
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)
find_package(Threads REQUIRED)

# grpc_cpp_plugin 是独立的可执行文件，不在 pkg-config 范围内，用 find_program 找实际路径
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
  message(FATAL_ERROR "grpc_cpp_plugin not found, please install protobuf-compiler-grpc")
endif()

set(PROTO_DIR ${CMAKE_SOURCE_DIR}/common/proto)

foreach(PROTO common healthcheck hotmethod control init)
  add_custom_command(
    OUTPUT
      ${CMAKE_BINARY_DIR}/${PROTO}.pb.cc ${CMAKE_BINARY_DIR}/${PROTO}.pb.h
      ${CMAKE_BINARY_DIR}/${PROTO}.grpc.pb.cc ${CMAKE_BINARY_DIR}/${PROTO}.grpc.pb.h
    COMMAND protoc
    ARGS --grpc_out=${CMAKE_BINARY_DIR} --cpp_out=${CMAKE_BINARY_DIR}
         -I${PROTO_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         ${PROTO_DIR}/${PROTO}.proto
    DEPENDS ${PROTO_DIR}/${PROTO}.proto
  )
  list(APPEND ALL_PROTO_SRCS
    ${CMAKE_BINARY_DIR}/${PROTO}.pb.cc
    ${CMAKE_BINARY_DIR}/${PROTO}.grpc.pb.cc)
endforeach()

file(GLOB COMMON_SRCS common/*.cpp)
add_library(drop_common ${COMMON_SRCS} ${ALL_PROTO_SRCS})
target_include_directories(drop_common PUBLIC
  ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/common
  ${GRPC_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
target_link_directories(drop_common PUBLIC ${GRPC_LIBRARY_DIRS} ${PROTOBUF_LIBRARY_DIRS})
target_link_libraries(drop_common PUBLIC ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES} Threads::Threads)
target_compile_options(drop_common PUBLIC ${GRPC_CFLAGS_OTHER} ${PROTOBUF_CFLAGS_OTHER})

file(GLOB SERVER_SRCS server/*.cpp)
add_executable(drop_server ${SERVER_SRCS})
target_link_libraries(drop_server PRIVATE drop_common)

file(GLOB AGENT_SRCS agent/*.cpp)
add_executable(drop_agent ${AGENT_SRCS})
target_link_libraries(drop_agent PRIVATE drop_common)

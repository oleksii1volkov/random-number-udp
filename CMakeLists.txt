cmake_minimum_required(VERSION 3.28)

project("udp")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.84 REQUIRED COMPONENTS system coroutine)
find_package(Protobuf REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/proto)

set(SOURCE_DIR src)
set(SERVER_SOURCE_DIR ${SOURCE_DIR}/server)
set(CLIENT_SOURCE_DIR ${SOURCE_DIR}/client)

file(GLOB_RECURSE SERVER_SOURCE_FILES "${SERVER_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE CLIENT_SOURCE_FILES "${CLIENT_SOURCE_DIR}/*.cpp")

set(INCLUDE_DIR include)
file(GLOB_RECURSE PROTO_FILES "${INCLUDE_DIR}/proto/*.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Server executable
add_executable(udp_server ${SERVER_SOURCE_FILES} ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(udp_server PUBLIC ${protobuf_INCLUDE_DIRS})
target_link_libraries(udp_server PRIVATE ${Boost_LIBRARIES} protobuf::libprotobuf)

# Client executable
add_executable(udp_client ${CLIENT_SOURCE_FILES} ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(udp_server PUBLIC ${protobuf_INCLUDE_DIRS})
target_link_libraries(udp_client PRIVATE ${Boost_LIBRARIES} protobuf::libprotobuf)

cmake_minimum_required(VERSION 3.22.1)

project("udp")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_C_COMPILER "cl.exe")
    set(CMAKE_CXX_COMPILER "cl.exe")
elseif(APPLE)
    set(CMAKE_C_COMPILER "clang-18")
    set(CMAKE_CXX_COMPILER "clang++-18")
elseif(UNIX AND NOT APPLE)
    set(CMAKE_C_COMPILER "clang-18")
    set(CMAKE_CXX_COMPILER "clang++-18")
endif()

find_package(Boost 1.84 REQUIRED COMPONENTS system coroutine)
find_package(Protobuf REQUIRED)

set(INCLUDE_DIR include)

include_directories(${INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/proto)

set(SOURCE_DIR src)
set(SERVER_SOURCE_DIR ${SOURCE_DIR}/server)
set(CLIENT_SOURCE_DIR ${SOURCE_DIR}/client)
set(UTILS_SOURCE_DIR ${SOURCE_DIR}/utils)

file(GLOB_RECURSE SERVER_SOURCE_FILES "${SERVER_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE CLIENT_SOURCE_FILES "${CLIENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE UTILS_SOURCE_FILES "${UTILS_SOURCE_DIR}/*.cpp")

file(GLOB_RECURSE PROTO_FILES "${INCLUDE_DIR}/proto/*.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
endif()

# Server executable
add_executable(udp_server ${SERVER_SOURCE_FILES} ${UTILS_SOURCE_FILES} ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(udp_server PUBLIC ${INCLUDE_DIR} ${Boost_INCLUDE_DIRS} ${protobuf_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include/proto)
target_link_libraries(udp_server PRIVATE ${Boost_LIBRARIES} protobuf::libprotobuf)

# Client executable
add_executable(udp_client ${CLIENT_SOURCE_FILES} ${UTILS_SOURCE_FILES} ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(udp_server PUBLIC ${INCLUDE_DIR} ${Boost_INCLUDE_DIRS} ${protobuf_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include/proto)
target_link_libraries(udp_client PRIVATE ${Boost_LIBRARIES} protobuf::libprotobuf)

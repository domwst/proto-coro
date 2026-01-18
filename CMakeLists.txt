cmake_minimum_required(VERSION 3.21)

project(proto_coro LANGUAGES CXX)

option(PROTO_CORO_BUILD_TESTS "Build proto_coro tests" ON)
option(PROTO_CORO_BUILD_EXAMPLES "Build proto_coro examples" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

include(cmake/Flags.cmake)

add_subdirectory(aux)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS lib/*.cpp)

add_library(proto_coro STATIC ${LIB_SOURCES})

target_include_directories(proto_coro PUBLIC lib)

target_compile_options(proto_coro PRIVATE -Wall -Wextra -Wpedantic)

if (PROTO_CORO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if (PROTO_CORO_BUILD_TESTS)
  add_subdirectory(tests)
endif()

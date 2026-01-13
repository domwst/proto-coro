cmake_minimum_required(VERSION 3.21)

project(proto_coro LANGUAGES CXX)

option(PROTO_CORO_BUILD_TESTS "Build proto_coro tests" ON)
option(PROTO_CORO_BUILD_EXAMPLES "Build proto_coro examples" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(proto_coro INTERFACE)

target_include_directories(proto_coro INTERFACE include)

set(CMAKE_CXX_STANDARD 23)

target_compile_options(proto_coro INTERFACE -Wall -Wextra -Wpedantic)

if (PROTO_CORO_BUILD_EXAMPLES)
  add_executable(proto_coro_demo examples/proto_coro_demo.cpp)
  target_link_libraries(proto_coro_demo PRIVATE proto_coro)
endif()

if (PROTO_CORO_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
  )
  FetchContent_MakeAvailable(Catch2)

  file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)
  add_executable(proto_coro_tests
    ${TEST_SOURCES}
  )
  target_link_libraries(proto_coro_tests PRIVATE proto_coro Catch2::Catch2WithMain)

  include(CTest)
  include(Catch)
  catch_discover_tests(proto_coro_tests)
endif()

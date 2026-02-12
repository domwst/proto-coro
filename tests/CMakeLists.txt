include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS *.cpp)
add_executable(proto_coro_tests
  ${TEST_SOURCES}
)
target_link_libraries(proto_coro_tests PRIVATE falter proto_coro Catch2::Catch2WithMain)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Tsan" AND NOT CMAKE_BUILD_TYPE STREQUAL "Asan")
  set(FTHREADS_LIB "${CMAKE_BINARY_DIR}/_deps/libfthreads.a")
  file(
    DOWNLOAD https://files.kek.today/libfthreads.a "${FTHREADS_LIB}"
    TLS_VERIFY ON
  )

  add_library(fthreads STATIC IMPORTED)
  set_target_properties(fthreads PROPERTIES IMPORTED_LOCATION "${FTHREADS_LIB}")

  add_executable(proto_coro_fthreads_tests test-fthreads.cpp)
  target_link_libraries(
    proto_coro_fthreads_tests
      PRIVATE proto_coro Catch2::Catch2WithMain fthreads
  )
endif()

include(CTest)
include(Catch)
catch_discover_tests(proto_coro_tests)

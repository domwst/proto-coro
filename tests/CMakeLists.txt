include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

file(GLOB_RECURSE COMMON_TEST_SOURCES CONFIGURE_DEPENDS common/*.cpp)
file(GLOB_RECURSE REAL_TEST_SOURCES CONFIGURE_DEPENDS real/*.cpp)
file(GLOB_RECURSE SIM_TEST_SOURCES CONFIGURE_DEPENDS sim/*.cpp)
add_executable(proto_coro_real_tests
  ${COMMON_TEST_SOURCES}
  ${REAL_TEST_SOURCES}
)
target_link_libraries(proto_coro_real_tests PRIVATE falter proto_coro Catch2::Catch2WithMain)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Tsan" AND NOT CMAKE_BUILD_TYPE STREQUAL "Asan")
  set(FTHREADS_LIB "${CMAKE_BINARY_DIR}/_deps/libfthreads.a")
  file(
    DOWNLOAD https://files.kek.today/libfthreads.a "${FTHREADS_LIB}"
    EXPECTED_HASH SHA256=4a02d05fa4a195fd96d1b7919c7fbdb2399f2a2281335eed5796151640ef0840
    TLS_VERIFY ON
  )

  add_library(fthreads STATIC IMPORTED)
  set_target_properties(fthreads PROPERTIES IMPORTED_LOCATION "${FTHREADS_LIB}")

  add_executable(proto_coro_sim_tests
    ${COMMON_TEST_SOURCES}
    ${SIM_TEST_SOURCES}
  )
  target_link_libraries(
    proto_coro_sim_tests
      PRIVATE proto_coro Catch2::Catch2WithMain fthreads
  )
endif()

include(CTest)
include(Catch)
catch_discover_tests(proto_coro_real_tests)
catch_discover_tests(proto_coro_sim_tests)

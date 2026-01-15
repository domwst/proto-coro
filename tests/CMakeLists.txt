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
target_link_libraries(proto_coro_tests PRIVATE proto_coro Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(proto_coro_tests)

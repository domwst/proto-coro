#include <catch2/catch_test_macros.hpp>

#include <proto-coro/pc.hpp>

namespace {

struct CounterCoro : Pc {
    int i = 0;

    PROTO_CORO(int) {
        PC_BEGIN;

        ++i;
        SUSPEND;

        ++i;
        SUSPEND;

        ++i;
        return i;

        PC_END;
    }
};

}  // namespace

TEST_CASE("Pc coroutine resumes and returns on completion") {
    CounterCoro coro;

    REQUIRE(coro.Step(nullptr) == std::nullopt);
    REQUIRE(coro.i == 1);

    REQUIRE(coro.Step(nullptr) == std::nullopt);
    REQUIRE(coro.i == 2);

    REQUIRE(coro.Step(nullptr).value() == 3);
    REQUIRE(coro.i == 3);
}

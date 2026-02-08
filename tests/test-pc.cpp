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
    struct DummyRuntime {
    } rt;

    CounterCoro coro;
    Context ctx{&coro, &rt};

    REQUIRE(coro.Step(&ctx) == std::nullopt);
    REQUIRE(coro.i == 1);

    REQUIRE(coro.Step(&ctx) == std::nullopt);
    REQUIRE(coro.i == 2);

    REQUIRE(coro.Step(&ctx).value() == 3);
    REQUIRE(coro.i == 3);
}

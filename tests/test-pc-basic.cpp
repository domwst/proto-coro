#include <catch2/catch_test_macros.hpp>

#include <proto-coro/pc.hpp>

namespace {

struct Coro : Pc {
    PROTO_CORO(int) {
        PC_BEGIN;

        SUSPEND;
        SUSPEND;
        return 123;

        PC_END;
    }
};

}  // namespace

TEST_CASE("PROTO_CORO coroutines resume across SUSPEND") {
    Coro c;

    CHECK(!c.Step(nullptr).has_value());
    CHECK(!c.Step(nullptr).has_value());
    CHECK(c.Step(nullptr).value() == 123);
}

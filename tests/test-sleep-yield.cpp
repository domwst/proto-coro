#include <proto-coro/concur-util.hpp>
#include <proto-coro/event-loop/event-loop.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/thread/event.hpp>

#include <falter/interface.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

namespace {

struct Coro : Pc {
    Coro() {
    }

    PROTO_CORO(int) {
        PC_BEGIN;

        ++counter;
        YIELD;
        ++counter;
        YIELD;
        ++counter;

        SLEEP_FOR(100ms);
        ++counter;
        return counter;

        PC_END;
    }

    int counter = 0;
};

TEST_CASE("Yield ans sleep") {
    ThreadOneshotEvent done;
    int counter;

    auto c = Spawn{Coro{} | FMap{[&done, &counter](auto&& res) {
                       counter = res;
                       done.Fire();
                       return Unit{};
                   }}};

    EventLoop loop{2};
    loop.Start();

    loop.Submit(&c);
    done.Wait();

    loop.Stop();

    REQUIRE(counter == 4);
    WARN("Falter stats: " << GlobalStats());
}

}  // namespace

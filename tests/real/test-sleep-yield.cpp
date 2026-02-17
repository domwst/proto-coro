#include <proto-coro/concur-util.hpp>
#include <proto-coro/event-loop/event-loop.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/thread/wait-group.hpp>

#include <falter/interface.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

namespace {

struct Coro : Pc {
    Coro(size_t iters) : iters(iters) {
    }

    PROTO_CORO(int) {
        PC_BEGIN;

        for (i = 0; i < iters; ++i) {
            ++counter;
            YIELD;
        }

        SLEEP_FOR(100ms);
        ++counter;

        return counter;

        PC_END;
    }

    int counter = 0;

    size_t iters;
    size_t i;
};

TEST_CASE("Yield ans sleep") {
    ThreadWaitGroup wg;

    EventLoop loop{2};
    loop.Start();

    constexpr size_t kCoros = 10;
    int counters[kCoros];
    for (size_t i = 0; i < kCoros; ++i) {
        wg.Add();
        auto c = new Spawn{Coro{i} | StoreResult(counters[i]) | ThenDone(wg) |
                               AndThen(SelfDestruct),
                           in<EventLoop>};

        loop.Submit(c);
    }

    wg.Wait();

    loop.Stop();

    for (size_t i = 0; i < kCoros; ++i) {
        REQUIRE(counters[i] == static_cast<int>(i + 1));
    }
    WARN("Falter stats: " << GlobalStats());
}

}  // namespace

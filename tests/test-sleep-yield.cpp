#include "proto-coro/event-loop/event-loop.hpp"
#include <catch2/catch_test_macros.hpp>

#include <proto-coro/concur-util.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/thread/event.hpp>

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace {

struct Coro : Pc {
    Coro() {
    }

    PROTO_CORO(std::vector<std::thread::id>) {
        PC_BEGIN;

        threads.push_back(std::this_thread::get_id());
        YIELD;
        threads.push_back(std::this_thread::get_id());
        YIELD;
        threads.push_back(std::this_thread::get_id());

        SLEEP_FOR(100ms);
        threads.push_back(std::this_thread::get_id());
        return std::move(threads);

        PC_END;
    }

    std::vector<std::thread::id> threads;
};

TEST_CASE("Yield ans sleep") {
    ThreadOneshotEvent done;
    std::vector<std::thread::id> threads;

    auto c = Spawn{Coro{} | FMap{[&done, &threads](auto&& res) {
                       threads = std::move(res);
                       done.Fire();
                       return Unit{};
                   }}};

    EventLoop loop{2};
    loop.Start();

    loop.Submit(&c);
    done.Wait();

    loop.Stop();

    REQUIRE(threads.size() == 4);
    std::ranges::sort(threads);
    threads.resize(std::unique(threads.begin(), threads.end()) -
                   threads.begin());
    WARN("Threads observed: " << threads.size());
}

}  // namespace

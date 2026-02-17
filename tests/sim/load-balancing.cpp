#include "loop.hpp"

#include <proto-coro/concur-util.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/thread/wait-group.hpp>

#include <catch2/catch_test_macros.hpp>
#include <random>

using namespace std::chrono_literals;

struct YieldSleepCoro : Pc {
    PROTO_CORO(Unit) {
        PC_BEGIN;

        for (; i < 3; ++i) {
            YIELD;
        }

        std::this_thread::sleep_for(1s);

        return Unit{};
        PC_END;
    }

    size_t i = 0;
};

template <class Runtime, class F>
static void SpawnWaitMultipleCoros(Runtime& loop, F coro, size_t num_coros) {
    ThreadWaitGroup wg;
    for (size_t i = 0; i < num_coros; ++i) {
        wg.Add();
        loop.Submit(new Spawn{coro() | ThenDone(wg) | AndThen(SelfDestruct),
                              in<Runtime>});
    }
    wg.Wait();
}

TEST_CASE("Load balancing") {
    constexpr size_t kThreads = 3;
    Loop loop{kThreads};
    loop.Start();

    for (size_t i = 0; i < 10'000; ++i) {
        auto groups = i % 4 + 1;
        auto start = std::chrono::steady_clock::now();
        SpawnWaitMultipleCoros(
            loop,
            []() {
                return YieldSleepCoro{};
            },
            groups * kThreads);
        auto end = std::chrono::steady_clock::now();
        auto expected = 1s * groups;
        REQUIRE(end - start >= expected);
        REQUIRE(end - start <= expected + 100ms);
    }

    loop.Stop();
}

struct WaitSleepCoro : Pc {
    PROTO_CORO(Unit) {
        PC_BEGIN;

        SLEEP_FOR(1s);

        std::this_thread::sleep_for(1s);

        return Unit{};

        PC_END;
    }
};

TEST_CASE("Sleep load balancing") {
    constexpr size_t kThreads = 3;
    Loop loop{kThreads};
    loop.Start();

    for (size_t i = 0; i < 10'000; ++i) {
        auto groups = i % 4 + 1;
        auto start = std::chrono::steady_clock::now();
        SpawnWaitMultipleCoros(
            loop,
            [] {
                return WaitSleepCoro{};
            },
            groups * kThreads);
        auto end = std::chrono::steady_clock::now();
        auto expected = 1s * groups + 1s;
        REQUIRE(end - start >= expected);
        REQUIRE(end - start <= expected + 100ms);
    }

    loop.Stop();
}

struct SleepingCoro : Pc {
    SleepingCoro(std::vector<std::chrono::nanoseconds> sleeps)
        : sleeps_(std::move(sleeps)) {
        std::reverse(sleeps_.begin(), sleeps_.end());
    }

    PROTO_CORO(Unit) {
        PC_BEGIN;

        while (!sleeps_.empty()) {
            std::chrono::nanoseconds sleep;
            sleep = sleeps_.back();
            sleeps_.pop_back();
            sleep_end_ = std::chrono::steady_clock::now() + sleep;
            SLEEP_UNTIL(sleep_end_);
            auto delay = std::chrono::steady_clock::now() - sleep_end_;
            REQUIRE(delay >= 0s);
            REQUIRE(delay <= 50ms);
        }

        return Unit{};

        PC_END;
    }

  private:
    std::chrono::steady_clock::time_point sleep_end_;
    std::vector<std::chrono::nanoseconds> sleeps_;
};

TEST_CASE("Sleep multiplexing") {
    std::mt19937 rng{424243};
    constexpr size_t kThreads = 3;
    Loop loop{kThreads};
    loop.Start();

    std::vector<std::chrono::nanoseconds> sleeps;
    for (size_t i = 0; i <= 10; ++i) {
        sleeps.push_back(1s * i);
    }

    for (size_t i = 0; i < 5'000; ++i) {
        auto start = std::chrono::steady_clock::now();
        auto spawned = rng() % (kThreads * 10) + 1;
        SpawnWaitMultipleCoros(
            loop,
            [sleeps, &rng] {
                auto s = sleeps;
                std::ranges::shuffle(s, rng);
                return SleepingCoro{std::move(s)};
            },
            spawned);
        auto elapsed = std::chrono::steady_clock::now() - start;
        REQUIRE(elapsed >= 55s);
        REQUIRE(elapsed <= 55s + 100ms);
    }

    loop.Stop();
}

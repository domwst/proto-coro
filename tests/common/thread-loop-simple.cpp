#include <proto-coro/concur-util.hpp>
#include <proto-coro/event-loop/thread-pool.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/thread/wait-group.hpp>

#include <catch2/catch_test_macros.hpp>

struct TP : ThreadPool<TP> {
    using ThreadPool<TP>::ThreadPool;

    void Start() {
        ThreadPool<TP>::Start(this);
    }
};

struct YieldCoro : Pc {
    YieldCoro(size_t cnt) : cnt(cnt) {
    }

    PROTO_CORO(Unit) {
        PC_BEGIN;

        for (; i < cnt; ++i) {
            YIELD;
        }

        return Unit{};

        PC_END;
    }

    size_t i = 0;
    size_t cnt;
};

TEST_CASE("ThreadPool") {
    TP tp(3);

    tp.Start();

    ThreadWaitGroup wg;

    for (size_t i = 0; i < 10; ++i) {
        wg.Add();
        tp.Submit(new SpawnDeleting{YieldCoro{10 + 20 * i} | FMap{[&wg](Unit) {
                                        wg.Done();
                                        return Unit{};
                                    }},
                                    in<TP>});
    }

    wg.Wait();

    tp.Stop();
}

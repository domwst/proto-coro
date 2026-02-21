#include <proto-coro/concur-util.hpp>
#include <proto-coro/event-loop/thread-pool.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/thread/wait-group.hpp>

#include <catch2/catch_test_macros.hpp>

struct TP : ThreadPool<TP> {
    using ThreadPool<TP>::ThreadPool;
    using RoutineAux = Unit;

    void Start() {
        ThreadPool<TP>::Start(this);
    }
};

struct YieldCoro : Pc {
    YieldCoro() : self(this) {
    }

    YieldCoro(size_t cnt_) : YieldCoro() {
        cnt = cnt_;
    }

    YieldCoro(YieldCoro&& other) : YieldCoro(other.cnt) {
    }

    PROTO_CORO(Unit) {
        PC_BEGIN;

        REQUIRE(self == this);
        for (; i < cnt; ++i) {
            YIELD;
            REQUIRE(self == this);
        }

        return Unit{};

        PC_END;
    }

    size_t i = 0;
    size_t cnt;
    YieldCoro* self;
};

TEST_CASE("ThreadPool") {
    TP tp(3);

    tp.Start();

    ThreadWaitGroup wg;

    for (size_t i = 0; i < 10; ++i) {
        wg.Add();
        tp.Submit(new Spawn{YieldCoro{10 + 20 * i} | ThenDone(wg) |
                                AndThen(SelfDestruct),
                            in<TP>});
    }

    wg.Wait();

    tp.Stop();
}

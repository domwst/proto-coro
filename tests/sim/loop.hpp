#pragma once

#include <proto-coro/event-loop/thread-pool.hpp>
#include <proto-coro/event-loop/timer-thread.hpp>

struct Loop : ThreadPool<Loop>, TimerThread<Loop> {
    using ThreadPool<Loop>::ThreadPool;

    void Start() {
        ThreadPool<Loop>::Start(this);
        TimerThread<Loop>::Start(this);
    }

    void Stop() {
        TimerThread<Loop>::Stop();
        ThreadPool<Loop>::Stop();
    }

    using TimerThread<Loop>::After;
    using ThreadPool<Loop>::Submit;
};

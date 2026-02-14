#pragma once

#include "mpsc-timer-queue.hpp"

#include <proto-coro/routine.hpp>

#include <thread>

template <class Runtime>
struct TimerThread {
    using Task = IRoutine<Runtime>;

    void Start(Runtime* rt) {
        timer_thread_ = std::thread(&TimerThread::TimerThreadBody, this, rt);
    }

    void After(TimePoint when, Task* routine) {
        timers_.Push(when, routine);
    }

    void Stop() {
        timers_.Close();
        timer_thread_.join();
    }

  private:
    void TimerThreadBody(Runtime* rt) {
        while (auto task = timers_.Pop()) {
            rt->Submit(*task);
        }
    }

    MPSCTimerQueue<Task*> timers_;
    std::thread timer_thread_;
};

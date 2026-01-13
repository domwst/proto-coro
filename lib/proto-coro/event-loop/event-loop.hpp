#pragma once

#include <proto-coro/routine.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/unused.hpp>

#include "mpmc-queue.hpp"
#include "mpsc-timer-queue.hpp"

#include <thread>
#include <vector>

struct EventLoop : IRuntime {
    EventLoop(size_t num_workers);

    void Start();

    void Stop();

    void Submit(IRoutine* routine) override;

    void After(TimePoint when, IRoutine* routine) override;

    void WhenReady(int fd, InterestKind type, IRoutine* routine) override;

  private:
    void WorkerThread();

    void TimerThread();

    std::vector<std::thread> workers_;
    MPMCQueue<IRoutine*> tasks_;

    std::thread timer_thread_;
    MPSCTimerQueue<IRoutine*> timers_;
};

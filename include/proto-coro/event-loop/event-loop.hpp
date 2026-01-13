#pragma once

#include <proto-coro/routine.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/unused.hpp>

#include "mpmc-queue.hpp"
#include "mpsc-timer-queue.hpp"

#include <thread>
#include <vector>

struct EventLoop : IRuntime {
    EventLoop(size_t num_workers) : workers_(num_workers) {
    }

    void Start() {
        for (auto& worker : workers_) {
            worker = std::thread(&EventLoop::WorkerThread, this);
        }
        timer_thread_ = std::thread(&EventLoop::TimerThread, this);
    }

    void Stop() {
        tasks_.Close();
        timers_.Close();
        for (auto& worker : workers_) {
            worker.join();
        }
        timer_thread_.join();
    }

    void Submit(IRoutine* routine) override {
        tasks_.Push(routine);
    }

    void After(TimePoint when, IRoutine* routine) override {
        std::this_thread::sleep_until(when);
        Submit(routine);
    }

    void WhenReady(int fd, InterestKind type, IRoutine* routine) override {
        UNUSED(fd, type, routine);
        throw std::runtime_error("Not implemented");
    }

  private:
    void WorkerThread() {
        while (auto task = tasks_.Pop()) {
            (*task)->Step(this);
            std::this_thread::yield();
        }
    }

    void TimerThread() {
        while (auto task = timers_.Pop()) {
            Submit(*task);
        }
    }

    std::vector<std::thread> workers_;
    MPMCQueue<IRoutine*> tasks_;

    std::thread timer_thread_;
    MPSCTimerQueue<IRoutine*> timers_;
};

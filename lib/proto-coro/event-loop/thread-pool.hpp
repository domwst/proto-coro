#pragma once

#include "mpmc-queue.hpp"

#include <proto-coro/routine.hpp>

#include <thread>
#include <vector>

template <class Runtime>
struct ThreadPool {
    using Task = IRoutine<Runtime>;

    explicit ThreadPool(size_t num_workers) : workers_(num_workers) {
    }

    void Start(Runtime* rt) {
        for (auto& worker : workers_) {
            worker = std::thread(&ThreadPool::WorkerThread, this, rt);
        }
    }

    void Stop() {
        tasks_.Close();
        for (auto& worker : workers_) {
            worker.join();
        }
    }

    void Submit(Task* routine) {
        tasks_.Push(routine);
    }

  private:
    void WorkerThread(Runtime* self) {
        while (auto task = tasks_.Pop()) {
            (*task)->Step(self);
        }
    }

    std::vector<std::thread> workers_;
    MPMCQueue<Task*> tasks_;
};

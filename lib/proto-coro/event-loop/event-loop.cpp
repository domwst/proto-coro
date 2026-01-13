#include "event-loop.hpp"

#include <stdexcept>

EventLoop::EventLoop(size_t num_workers) : workers_(num_workers) {
}

void EventLoop::Start() {
    for (auto& worker : workers_) {
        worker = std::thread(&EventLoop::WorkerThread, this);
    }
    timer_thread_ = std::thread(&EventLoop::TimerThread, this);
}

void EventLoop::Stop() {
    tasks_.Close();
    timers_.Close();
    for (auto& worker : workers_) {
        worker.join();
    }
    timer_thread_.join();
}

void EventLoop::Submit(IRoutine* routine) {
    tasks_.Push(routine);
}

void EventLoop::After(TimePoint when, IRoutine* routine) {
    std::this_thread::sleep_until(when);
    Submit(routine);
}

void EventLoop::WhenReady(int /*fd*/, InterestKind /*type*/,
                          IRoutine* /*routine*/) {
    throw std::runtime_error("Not implemented");
}

void EventLoop::WorkerThread() {
    while (auto task = tasks_.Pop()) {
        (*task)->Step(this);
        std::this_thread::yield();
    }
}

void EventLoop::TimerThread() {
    while (auto task = timers_.Pop()) {
        Submit(*task);
    }
}

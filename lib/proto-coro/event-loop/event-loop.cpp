#include "event-loop.hpp"

#include "mpmc-queue.hpp"
#include "mpsc-timer-queue.hpp"

#include <proto-coro/unused.hpp>

#include <stdexcept>
#include <thread>
#include <vector>

struct EventLoop::Impl {
    Impl(size_t num_workers) : workers_(num_workers) {
    }

    void Start(EventLoop* self) {
        for (auto& worker : workers_) {
            worker = std::thread(&Impl::WorkerThread, this, self);
        }
        timer_thread_ = std::thread(&Impl::TimerThread, this);
    }

    void Stop() {
        tasks_.Close();
        timers_.Close();
        for (auto& worker : workers_) {
            worker.join();
        }
        timer_thread_.join();
    }

    void Submit(IRoutine* routine) {
        tasks_.Push(routine);
    }

    void After(TimePoint when, IRoutine* routine) {
        timers_.Push(when, routine);
    }

    void WhenReady(int fd, InterestKind type, IRoutine* routine) {
        UNUSED(fd, type, routine);
        throw std::runtime_error("Not implemented");
    }

  private:
    void WorkerThread(EventLoop* self) {
        while (auto task = tasks_.Pop()) {
            (*task)->Step(self);
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

EventLoop::EventLoop(size_t num_workers) : impl_(num_workers) {
}

void EventLoop::Start() {
    impl_->Start(this);
}

void EventLoop::Stop() {
    impl_->Stop();
}

void EventLoop::Submit(IRoutine* routine) {
    impl_->Submit(routine);
}

void EventLoop::After(TimePoint when, IRoutine* routine) {
    impl_->After(when, routine);
}

void EventLoop::WhenReady(int fd, InterestKind type, IRoutine* routine) {
    impl_->WhenReady(fd, type, routine);
}

EventLoop::~EventLoop() = default;

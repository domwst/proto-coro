#include "event-loop.hpp"

#include "epoll.hpp"
#include "fail.hpp"
#include "mpmc-queue.hpp"
#include "mpsc-timer-queue.hpp"

#include <proto-coro/unused.hpp>

#include <sys/epoll.h>
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
        epoll_thread_ = std::thread(&Impl::EpollThread, this);
    }

    void Stop() {
        tasks_.Close();
        timers_.Close();
        epoll_.Close();
        for (auto& worker : workers_) {
            worker.join();
        }
        timer_thread_.join();
        epoll_thread_.join();
    }

    void Submit(IRoutine* routine) {
        tasks_.Push(routine);
    }

    void After(TimePoint when, IRoutine* routine) {
        timers_.Push(when, routine);
    }

    void RegisterFd(int fd) {
        if (epoll_.Register(fd, 0, nullptr) < 0) {
            Fail("register fd");
        }
    }

    void DeregisterFd(int fd) {
        if (epoll_.Deregister(fd) < 0) {
            Fail("deregister fd");
        }
    }

    void WhenReady(int fd, InterestKind type, IRoutine* routine) {
        uint32_t epoll_flags = 0;
        auto utype = static_cast<uint8_t>(type);
        if (utype & static_cast<uint8_t>(InterestKind::Readable)) {
            epoll_flags |= EPOLLIN;
        }
        if (utype & static_cast<uint8_t>(InterestKind::Writeable)) {
            epoll_flags |= EPOLLOUT;
        }

        if (epoll_.Modify(fd, epoll_flags, routine) < 0) {
            Fail("modify fd");
        }
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

    void EpollThread() {
        std::pair<uint32_t, void*> buf[16];
        auto s = std::span{buf};
        while (auto tasks = epoll_.Poll(-1, s)) {
            for (auto& task : s.first(*tasks)) {
                Submit(static_cast<IRoutine*>(task.second));
            }
        }
    }

    std::vector<std::thread> workers_;
    MPMCQueue<IRoutine*> tasks_;

    std::thread timer_thread_;
    MPSCTimerQueue<IRoutine*> timers_;

    std::thread epoll_thread_;
    Epoll epoll_;
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

void EventLoop::RegisterFd(int fd) {
    impl_->RegisterFd(fd);
}

void EventLoop::DeregisterFd(int fd) {
    impl_->DeregisterFd(fd);
}

void EventLoop::WhenReady(int fd, InterestKind type, IRoutine* routine) {
    impl_->WhenReady(fd, type, routine);
}

EventLoop::~EventLoop() = default;

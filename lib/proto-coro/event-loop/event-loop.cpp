#include "event-loop.hpp"

#include "epoll.hpp"
#include "fail.hpp"
#include "thread-pool.hpp"
#include "timer-thread.hpp"

#include <proto-coro/unused.hpp>

#include <sys/epoll.h>
#include <thread>

#ifdef TSAN
extern "C" {
void __tsan_acquire(void* ptr);
void __tsan_release(void* ptr);
}

static void Acquire(void* ptr) {
    __tsan_acquire(ptr);
}

static void Release(void* ptr) {
    __tsan_release(ptr);
}
#else
static void Acquire(void*) {
}

static void Release(void*) {
}
#endif

struct EventLoop::Impl : ThreadPool<EventLoop>, TimerThread<EventLoop> {
    Impl(size_t num_workers) : ThreadPool<EventLoop>(num_workers) {
    }

    void Start(EventLoop* self) {
        ThreadPool<EventLoop>::Start(self);
        TimerThread<EventLoop>::Start(self);
        epoll_thread_ = std::thread(&Impl::EpollThread, this);
    }

    void Stop() {
        epoll_.Close();
        epoll_thread_.join();

        TimerThread<EventLoop>::Stop();
        ThreadPool<EventLoop>::Stop();
    }

    using TimerThread<EventLoop>::After;
    using ThreadPool<EventLoop>::Submit;

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

    void WhenReady(int fd, InterestKind type, EventLoop::Task* routine) {
        Release(routine);
        uint32_t epoll_flags = EPOLLONESHOT;
        auto utype = static_cast<uint8_t>(type);
        if (utype & static_cast<uint8_t>(InterestKind::Readable)) {
            epoll_flags |= EPOLLIN;
        }
        if (utype & static_cast<uint8_t>(InterestKind::Writable)) {
            epoll_flags |= EPOLLOUT;
        }

        if (epoll_.Modify(fd, epoll_flags, routine) < 0) {
            Fail("modify fd");
        }
    }

  private:
    void EpollThread() {
        std::pair<uint32_t, void*> buf[16];
        auto s = std::span{buf};
        while (auto tasks = epoll_.Poll(-1, s)) {
            for (auto& task : s.first(*tasks)) {
                Acquire(task.second);
                Submit(static_cast<EventLoop::Task*>(task.second));
            }
        }
    }

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

void EventLoop::Submit(EventLoop::Task* routine) {
    impl_->Submit(routine);
}

void EventLoop::After(TimePoint when, EventLoop::Task* routine) {
    impl_->After(when, routine);
}

void EventLoop::Register(int fd) {
    impl_->RegisterFd(fd);
}

void EventLoop::Deregister(int fd) {
    impl_->DeregisterFd(fd);
}

void EventLoop::WhenReady(int fd, InterestKind type, EventLoop::Task* routine) {
    impl_->WhenReady(fd, type, routine);
}

EventLoop::~EventLoop() = default;

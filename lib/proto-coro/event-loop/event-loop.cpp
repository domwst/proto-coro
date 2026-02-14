#include "event-loop.hpp"

#include "epoll-thread.hpp"
#include "thread-pool.hpp"
#include "timer-thread.hpp"

#include <proto-coro/unused.hpp>

#include <sys/epoll.h>

struct EventLoop::Impl : ThreadPool<EventLoop>,
                         TimerThread<EventLoop>,
                         EpollThread<EventLoop> {
    Impl(size_t num_workers) : ThreadPool<EventLoop>(num_workers) {
    }

    void Start(EventLoop* self) {
        ThreadPool<EventLoop>::Start(self);
        TimerThread<EventLoop>::Start(self);
        EpollThread<EventLoop>::Start(self);
    }

    void Stop() {
        EpollThread<EventLoop>::Stop();
        TimerThread<EventLoop>::Stop();
        ThreadPool<EventLoop>::Stop();
    }
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

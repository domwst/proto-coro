#pragma once

#include "epoll.hpp"
#include "fail.hpp"

#include <proto-coro/routine.hpp>
#include <proto-coro/rt.hpp>

#include <sys/epoll.h>
#include <thread>

template <class Runtime>
struct EpollThread {
    using Task = IRoutine<Runtime>;

    void Start(Runtime* self) {
        epoll_.emplace();
        epoll_thread_ = std::thread(&EpollThread::EpollThreadBody, this, self);
    }

    void Stop() {
        epoll_.value().Close();
        epoll_thread_.join();
    }

    void RegisterFd(int fd) {
        if (epoll_.value().Register(fd, 0, nullptr) < 0) {
            Fail("register fd");
        }
    }

    void DeregisterFd(int fd) {
        if (epoll_.value().Deregister(fd) < 0) {
            Fail("deregister fd");
        }
    }

    void WhenReady(int fd, InterestKind type, Task* routine) {
        uint32_t epoll_flags = EPOLLONESHOT;
        auto utype = static_cast<uint8_t>(type);
        if (utype & static_cast<uint8_t>(InterestKind::Readable)) {
            epoll_flags |= EPOLLIN;
        }
        if (utype & static_cast<uint8_t>(InterestKind::Writable)) {
            epoll_flags |= EPOLLOUT;
        }

        if (epoll_.value().Modify(fd, epoll_flags, routine) < 0) {
            Fail("modify fd");
        }
    }

  private:
    void EpollThreadBody(Runtime* self) {
        std::pair<uint32_t, void*> buf[16];
        auto s = std::span{buf};
        auto& epoll = epoll_.value();
        while (auto tasks = epoll.Poll(-1, s)) {
            for (auto& task : s.first(*tasks)) {
                self->Submit(static_cast<Task*>(task.second));
            }
        }
    }

    std::thread epoll_thread_;
    std::optional<Epoll> epoll_;
};

#include "epoll.hpp"
#include "fail.hpp"

#include <sys/epoll.h>

Epoll::Epoll() {
    int efd = epoll_create1(EPOLL_CLOEXEC);
    if (efd < 0) {
        Fail("create epoll fd");
    }
    efd_ = OwnedFd::FromRaw(efd);
}

int Epoll::Register(int fd, uint32_t flags, void* cookie) {
    return Change(fd, flags, cookie, EPOLL_CTL_ADD);
}

int Epoll::Modify(int fd, uint32_t flags, void* cookie) {
    return Change(fd, flags, cookie, EPOLL_CTL_MOD);
}

int Epoll::Deregister(int fd) {
    return epoll_ctl(efd_.GetRaw(), EPOLL_CTL_DEL, fd, nullptr);
}

size_t Epoll::Poll(int timeout_ms,
                   std::span<std::pair<uint32_t, void*>> tasks_buf) {
    constexpr size_t kMaxEvents = 16;
    epoll_event events[kMaxEvents];

    ssize_t tasks =
        epoll_wait(efd_.GetRaw(), events,
                   std::min(kMaxEvents, tasks_buf.size()), timeout_ms);
    if (tasks < 0) {
        Fail("epoll wait");
    }

    for (auto& event : std::span{events, events + tasks}) {
        tasks_buf.front() = {event.events, event.data.ptr};
        tasks_buf = tasks_buf.subspan(1);
    }
    return tasks;
}

int Epoll::Change(int fd, uint32_t flags, Task* task, int cmd) {
    epoll_event ev{
        .events = flags,
        .data =
            {
                .ptr = task,
            },
    };
    return epoll_ctl(efd_.GetRaw(), cmd, fd, &ev);
}

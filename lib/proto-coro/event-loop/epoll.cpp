#include "epoll.hpp"
#include "fail.hpp"

#include <cassert>
#include <cstring>
#include <sys/epoll.h>
#include <sys/eventfd.h>

Epoll::Epoll() {
    int efd = epoll_create1(EPOLL_CLOEXEC);
    if (efd < 0) {
        Fail("create epoll fd");
    }
    efd_ = OwnedFd::FromRaw(efd);

    int closed_event_fd = eventfd(0, EFD_CLOEXEC);
    if (closed_event_fd < 0) {
        Fail("create event fd");
    }
    closed_event_fd_ = OwnedFd::FromRaw(closed_event_fd);

    if (Register(closed_event_fd_.AsRawFd(), EPOLLIN, nullptr) < 0) {
        Fail("register event fd");
    }
}

int Epoll::Register(int fd, uint32_t flags, void* cookie) {
    return Change(fd, flags, cookie, EPOLL_CTL_ADD);
}

int Epoll::Modify(int fd, uint32_t flags, void* cookie) {
    return Change(fd, flags, cookie, EPOLL_CTL_MOD);
}

int Epoll::Deregister(int fd) {
    return epoll_ctl(efd_.AsRawFd(), EPOLL_CTL_DEL, fd, nullptr);
}

static void* ReadDataPtr(const epoll_event& event) {
    void* res;
    memcpy(&res,
           reinterpret_cast<const char*>(&event) + offsetof(epoll_event, data),
           sizeof(res));
    return res;
}

std::optional<size_t>
Epoll::Poll(int timeout_ms, std::span<std::pair<uint32_t, void*>> tasks_buf) {
    if (is_closed_.load(std::memory_order_relaxed)) {
        return std::nullopt;
    }
    constexpr size_t kMaxEvents = 16;
    epoll_event events[kMaxEvents];

    ssize_t tasks =
        epoll_wait(efd_.AsRawFd(), events,
                   std::min(kMaxEvents, tasks_buf.size()), timeout_ms);
    if (tasks < 0) {
        Fail("epoll wait");
    }

    for (auto& event : std::span{events, events + tasks}) {
        auto ptr = ReadDataPtr(event);
        if (!ptr) {
            assert(tasks == 1);
            --tasks;
            continue;
        }
        tasks_buf.front() = {event.events, ptr};
        tasks_buf = tasks_buf.subspan(1);
    }
    return tasks;
}

int Epoll::Change(int fd, uint32_t flags, void* task, int cmd) {
    epoll_event ev{
        .events = flags,
        .data =
            {
                .ptr = task,
            },
    };
    return epoll_ctl(efd_.AsRawFd(), cmd, fd, &ev);
}

void Epoll::Close() {
    is_closed_.store(true, std::memory_order_relaxed);
    uint64_t buf = 1;
    // Release
    if (write(closed_event_fd_.AsRawFd(), &buf, sizeof(buf)) < 0) {
        Fail("close epoll");
    }
}

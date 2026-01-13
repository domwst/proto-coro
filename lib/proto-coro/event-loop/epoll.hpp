#pragma once

#include "owned-fd.hpp"

#include <span>

struct Epoll {
    Epoll();

    [[nodiscard]] int Register(int fd, uint32_t flags, void* cookie);

    [[nodiscard]] int Modify(int fd, uint32_t flags, void* cooke);

    [[nodiscard]] int Deregister(int fd);

    size_t Poll(int timeout_ms,
                std::span<std::pair<uint32_t, void*>> tasks_buf);

  private:
    [[nodiscard]] int Change(int fd, uint32_t flags, void* task, int cmd);

    OwnedFd efd_;
};

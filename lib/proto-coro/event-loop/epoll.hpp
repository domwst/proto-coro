#pragma once

#include "owned-fd.hpp"

#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

struct Epoll {
    Epoll();

    [[nodiscard]] int Register(int fd, uint32_t flags, void* cookie);

    [[nodiscard]] int Modify(int fd, uint32_t flags, void* cooke);

    [[nodiscard]] int Deregister(int fd);

    std::optional<size_t> Poll(int timeout_ms,
                               std::span<std::pair<uint32_t, void*>> tasks_buf);

    void Close();

  private:
    [[nodiscard]] int Change(int fd, uint32_t flags, void* cookie, int cmd);

    std::atomic<bool> is_closed_ = false;
    OwnedFd efd_;
    OwnedFd closed_event_fd_;
};

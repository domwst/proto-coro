#pragma once

#include <unistd.h>
#include <utility>

struct OwnedFd {
    OwnedFd() = default;

    OwnedFd(const OwnedFd&) = delete;
    OwnedFd& operator=(const OwnedFd&) = delete;

    OwnedFd(OwnedFd&& other) noexcept : fd_(other.Release()) {
    }

    OwnedFd& operator=(OwnedFd&& other) noexcept {
        OwnedFd tmp{std::move(other)};
        Swap(tmp);
        return *this;
    }

    static OwnedFd FromRaw(int fd) {
        return OwnedFd(fd);
    }

    void Swap(OwnedFd& other) noexcept {
        std::swap(fd_, other.fd_);
    }

    int AsRawFd() const {
        return fd_;
    }

    int Release() {
        return std::exchange(fd_, kInvalidFd);
    }

    void Reset() {
        int fd = Release();
        if (IsValidFd(fd)) {
            ::close(fd);
        }
    }

    bool IsValid() const {
        return IsValidFd(fd_);
    }

    ~OwnedFd() {
        Reset();
    }

  private:
    static constexpr int kInvalidFd = -1;

    static bool IsValidFd(int fd) {
        return fd != kInvalidFd;
    }

    OwnedFd(int fd) : fd_(fd) {
    }

    int fd_ = kInvalidFd;
};

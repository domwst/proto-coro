#pragma once

#include "owned-fd.hpp"

#include <proto-coro/rt.hpp>

struct RegisteredFd : private OwnedFd {
    RegisteredFd(OwnedFd fd, IRuntime* rt) : OwnedFd(std::move(fd)), rt_(rt) {
        rt_->RegisterFd(AsRawFd());
    }

    RegisteredFd(RegisteredFd&& other) noexcept = default;
    RegisteredFd& operator=(RegisteredFd&& other) noexcept = default;

    using OwnedFd::AsRawFd;
    using OwnedFd::IsValid;

    OwnedFd IntoOwned() && {
        OwnedFd fd{};
        fd.Swap(*this);
        if (fd.IsValid()) {
            std::exchange(rt_, nullptr)->DeregisterFd(fd.AsRawFd());
        }
        return fd;
    }

    void Reset() {
        std::move(*this).IntoOwned();
    }

    void Swap(RegisteredFd& other) noexcept {
        std::swap(rt_, other.rt_);
        OwnedFd::Swap(other);
    }

    ~RegisteredFd() {
        std::move(*this).IntoOwned();
    }

  private:
    IRuntime* rt_ = nullptr;
};

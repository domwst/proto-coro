#pragma once

#include "fd-registerer.hpp"
#include "owned-fd.hpp"

#include <proto-coro/rt.hpp>

struct RegisteredFd : private OwnedFd {
    RegisteredFd(OwnedFd fd, IFdRegisterer* registry)
        : OwnedFd(std::move(fd)), registry_(registry) {
        registry_->Register(AsRawFd());
    }

    RegisteredFd(RegisteredFd&& other) noexcept = default;
    RegisteredFd& operator=(RegisteredFd&& other) noexcept = default;

    using OwnedFd::AsRawFd;
    using OwnedFd::IsValid;

    OwnedFd IntoOwned() && {
        OwnedFd fd{};
        fd.Swap(*this);
        if (fd.IsValid()) {
            std::exchange(registry_, nullptr)->Deregister(fd.AsRawFd());
        }
        return fd;
    }

    void Reset() {
        std::move(*this).IntoOwned();
    }

    void Swap(RegisteredFd& other) noexcept {
        std::swap(registry_, other.registry_);
        OwnedFd::Swap(other);
    }

    ~RegisteredFd() {
        std::move(*this).IntoOwned();
    }

  private:
    IFdRegisterer* registry_ = nullptr;
};

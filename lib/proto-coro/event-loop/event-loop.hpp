#pragma once

#include <proto-coro/fast-pimpl.hpp>
#include <proto-coro/routine.hpp>
#include <proto-coro/rt.hpp>

struct EventLoop : IRuntime {
    EventLoop(size_t num_workers);

    void Start();

    void Stop();

    void Submit(IRoutine* routine) override;

    void After(TimePoint when, IRoutine* routine) override;

    void RegisterFd(int fd) override;
    void DeregisterFd(int fd) override;
    void WhenReady(int fd, InterestKind type, IRoutine* routine) override;

    ~EventLoop();

  private:
    struct Impl;
    FastPimpl<Impl, 360, 8> impl_;
};

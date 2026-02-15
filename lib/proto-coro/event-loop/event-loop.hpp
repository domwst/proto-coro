#pragma once

#include <proto-coro/fast-pimpl.hpp>
#include <proto-coro/routine.hpp>
#include <proto-coro/rt.hpp>

struct EventLoop {
    using Task = IRoutine<EventLoop>;

    EventLoop(size_t num_workers);

    void Start();

    void Stop();

    void Submit(Task* routine);

    void After(TimePoint when, Task* routine);

    void Register(int fd);
    void Deregister(int fd);
    void WhenReady(int fd, InterestKind type, Task* routine);

    ~EventLoop();

  private:
    struct Impl;
    FastPimpl<Impl, 360, 8> impl_;
};

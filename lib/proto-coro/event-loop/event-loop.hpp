#pragma once

#include <proto-coro/fast-pimpl.hpp>
#include <proto-coro/routine.hpp>
#include <proto-coro/rt.hpp>
#include <proto-coro/unit.hpp>

struct EventLoop {
    using Task = IRoutine<EventLoop>;
    using RoutineAux = Unit;

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
    FastPimpl<Impl, 896, 128> impl_;
};

#pragma once

#include "routine.hpp"
#include "rt.hpp"

#include <concepts>

template <class Runtime>
concept Executor = requires(Runtime* rt, IRoutine<Runtime>* routine) {
    { rt->Submit(routine) } -> std::same_as<void>;
};

template <class Runtime>
concept TimersManager =
    requires(Runtime* rt, IRoutine<Runtime>* routine, TimePoint tp) {
        { rt->After(tp, routine) } -> std::same_as<void>;
    };

template <class Runtime>
concept IOManager = requires(Runtime* rt, IRoutine<Runtime>* routine, RawFd fd,
                             InterestKind ik) {
    { rt->Register(fd) } -> std::same_as<void>;
    { rt->Deregister(fd) } -> std::same_as<void>;
    { rt->WhenReady(fd, ik, routine) } -> std::same_as<void>;
};

#pragma once

#include "ctx.hpp"

#include <chrono>
#include <cstdint>

using RawFd = int;

enum class InterestKind : uint8_t {
    None = 0b00,
    Readable = 0b01,
    Writeable = 0b10,
};

inline InterestKind operator|(InterestKind lhs, InterestKind rhs) {
    return static_cast<InterestKind>(static_cast<uint8_t>(lhs) |
                                     static_cast<uint8_t>(rhs));
}

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

struct IRuntime {
    virtual void Submit(IRoutine* routine) = 0;

    virtual void After(TimePoint when, IRoutine* routine) = 0;

    virtual void WhenReady(RawFd fd, InterestKind type, IRoutine* routine) = 0;
};

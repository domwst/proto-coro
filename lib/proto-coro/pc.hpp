#pragma once

#include "ctx.hpp"

#include <algorithm>
#include <cstdint>
#include <type_traits>

using State = uint16_t;

constexpr State kInitialState = -1;

struct Pc {
    State pc_state = kInitialState;
};

#define _CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) _CONCAT_IMPL(a, b)

#define UNIQUE_ID(id) CONCAT(id, __COUNTER__)

#define _SUSPEND_START(label) this->pc_state = label
#define _SUSPEND_END(label)                                                    \
    return std::nullopt;                                                       \
    case label:

#define _SUSPEND_IMPL(label, block)                                            \
    _SUSPEND_START(label);                                                     \
    block;                                                                     \
    _SUSPEND_END(label)

#define SUSPEND_AND(block) _SUSPEND_IMPL(__COUNTER__, block)
#define SUSPEND SUSPEND_AND({})

#define PC_BEGIN                                                               \
    switch (this->pc_state) {                                                  \
    case kInitialState:

#define PC_END                                                                 \
    default:                                                                   \
        std::abort();                                                          \
        }

#define _CALLEE_PTR(callable_t)                                                \
    reinterpret_cast<callable_t*>(this->pc_callee_storage.Get())

#define _RESULT_PTR(storage, callable_t)                                       \
    reinterpret_cast<OutputOf<callable_t>*>(storage.Get())

#define CTX_VAR pc_ctx

#define _POLL(label, result_storage, callable_t, result, coro)                 \
    StorageFor<OutputOf<callable_t>> result_storage;                           \
    while (true) {                                                             \
        bool finished;                                                         \
        _SUSPEND_START(label);                                                 \
        {                                                                      \
            auto res = coro.Step(CTX_VAR);                                     \
            finished = res.has_value();                                        \
            if (finished) {                                                    \
                new (result_storage.template Get<OutputOf<callable_t>>())      \
                    OutputOf<callable_t>(std::move(*res));                     \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        _SUSPEND_END(label);                                                   \
    }                                                                          \
    result = std::move(*_RESULT_PTR(result_storage, callable_t));              \
    _RESULT_PTR(result_storage, callable_t)->~OutputOf<callable_t>()

#define POLL(result, coro)                                                     \
    _POLL(__COUNTER__, UNIQUE_ID(result_storage), decltype(coro), result, coro)

#define CALL(result, callable_t, ...)                                          \
    new (this->pc_callee_storage.Get<callable_t>()) callable_t(__VA_ARGS__);   \
    _POLL(__COUNTER__, UNIQUE_ID(result_storage), callable_t, result,          \
          (*_CALLEE_PTR(callable_t)));                                         \
    _CALLEE_PTR(callable_t)->~callable_t()

#define CALL_DISCARD(callable_t, ...)                                          \
    CALL(auto UNIQUE_ID(discard), callable_t, __VA_ARGS__)

#define EMPTY

#define PROTO_CORO_IMPL(ret, where)                                            \
    std::optional<ret> where Step([[maybe_unused]] const Context* CTX_VAR)

#define PROTO_CORO(ret) PROTO_CORO_IMPL(ret, EMPTY)

template <class T>
using OutputOf = std::remove_cvref_t<decltype(*std::declval<T>().Step(
    std::declval<const Context*>()))>;

// A-la aligned union
template <class... Ts>
struct StorageFor {
    constexpr static size_t kAlignment = std::max({alignof(Ts)...});
    constexpr static size_t kSize = std::max({sizeof(Ts)...});

    alignas(kAlignment) char data_[kSize];

    template <class T>  // Optional safety check
    void* Get() {
        static_assert(sizeof(T) <= kSize && alignof(T) <= kAlignment);
        return Get();
    }

    void* Get() {
        return data_;
    }
};

#define CALLS(...) StorageFor<__VA_ARGS__> pc_callee_storage

#pragma once

#include "ctx.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>  // IWYU pragma: keep  // std::destroy_at is used in macro expansion
#include <type_traits>
#include <utility>

using State = uint16_t;

template <class T>
using InnerOptional = std::remove_cvref_t<decltype(*std::declval<T>())>;

template <class T>
using OutputOf = std::remove_cvref_t<decltype(*std::declval<T>().Step(
    std::declval<const Context*>()))>;

constexpr State kInitialState = 0;

struct Pc {
    State pc_state = kInitialState;
};

// To be used instead of void
struct Unit {};

#define _CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) _CONCAT_IMPL(a, b)

#define UNIQUE_ID(id) CONCAT(id, __COUNTER__)

#define _SUSPEND_START(label) this->pc_state = label + 1
#define _SUSPEND_END(label)                                                    \
    return std::nullopt;                                                       \
    case label + 1:

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

#define _RESULT_PTR(storage, result_t)                                         \
    reinterpret_cast<result_t*>(storage.Get())

#define CTX_VAR pc_ctx

#define _READY(tmp, res, expr)                                                 \
    auto tmp = expr;                                                           \
    if (!tmp.has_value()) {                                                    \
        return std::nullopt;                                                   \
    }                                                                          \
    res = std::move(*tmp);

#define READY(res, expr) _READY(UNIQUE_ID(tmp), res, expr)
#define READY_DISCARD(expr)                                                    \
    READY([[maybe_unused]] auto UNIQUE_ID(discard), expr)

#define _POLL(label, result_storage, result_t, result, expr)                   \
    StorageFor<result_t> result_storage;                                       \
    _SUSPEND_START(label);                                                     \
    while (true) {                                                             \
        bool finished;                                                         \
        {                                                                      \
            auto res = expr;                                                   \
            finished = res.has_value();                                        \
            if (finished) {                                                    \
                new (result_storage.template Get<result_t>())                  \
                    result_t(std::move(*res));                                 \
                break;                                                         \
            }                                                                  \
        }                                                                      \
        _SUSPEND_END(label);                                                   \
    }                                                                          \
    result = std::move(*_RESULT_PTR(result_storage, result_t));                \
    std::destroy_at(_RESULT_PTR(result_storage, result_t))

#define POLL(result, expr)                                                     \
    _POLL(__COUNTER__, UNIQUE_ID(result_storage),                              \
          InnerOptional<decltype(expr)>, result, expr)

#define POLL_DISCARD(expr)                                                     \
    {                                                                          \
        POLL([[maybe_unused]] auto UNIQUE_ID(discard), expr);                  \
    }

#define POLL_CORO(result, coro) POLL(result, (coro).Step(CTX_VAR))

#define _CALLEE_PTR(callable_t)                                                \
    reinterpret_cast<callable_t*>(this->pc_callee_storage.Get())

#define _CALL(callable_t, result, ...)                                         \
    new (this->pc_callee_storage.Get<callable_t>()) __VA_ARGS__;               \
    POLL_CORO(result, *_CALLEE_PTR(callable_t));                               \
    std::destroy_at(_CALLEE_PTR(callable_t))

#define CALL(result, ...) _CALL(decltype(__VA_ARGS__), result, __VA_ARGS__)

#define CALL_DISCARD(...)                                                      \
    {                                                                          \
        CALL([[maybe_unused]] auto UNIQUE_ID(discard), __VA_ARGS__);           \
    }

#define EMPTY

#define PROTO_CORO_IMPL(ret, where)                                            \
    std::optional<ret> where Step([[maybe_unused]] const Context* CTX_VAR)

#define PROTO_CORO(ret) PROTO_CORO_IMPL(ret, EMPTY)

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

template <class... Ts>
struct CalleeStorageFor : StorageFor<Ts...> {
    CalleeStorageFor() {
        std::ranges::fill(this->data_, 0);
    }
};

#define CALLS(...) CalleeStorageFor<__VA_ARGS__> pc_callee_storage

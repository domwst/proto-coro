#pragma once

#include "pc.hpp"
#include "routine.hpp"
#include "rt.hpp"

#include <memory>

template <class Inner>
struct Boxed {
    template <class... Args>
    Boxed(Args&&... args)
        : inner(std::make_unique<Inner>(std::forward<Args>(args)...)) {
    }

    PROTO_CORO(OutputOf<Inner>) {
        return inner->Step(CTX_VAR);
    }

  private:
    std::unique_ptr<Inner> inner;
};

template <class T>
struct Spawn : IRoutine {
    Spawn(T&& routine) : inner(std::move(routine)) {
    }

    void Step(IRuntime* rt) override {
        Context ctx{this, rt};
        inner.Step(&ctx);
    }

  private:
    T inner;
};

#define _SLEEP_UNTIL_IMPL(label, when)                                         \
    _SUSPEND_START(label);                                                     \
    CTX_VAR->rt->After(when, CTX_VAR->self);                                   \
    _SUSPEND_END(label)

#define SLEEP_UNTIL(when) _SLEEP_UNTIL_IMPL(__COUNTER__, when)
#define SLEEP_FOR(duration) SLEEP_UNTIL(Clock::now() + duration)

#define _YIELD_IMPL(label)                                                     \
    _SUSPEND_START(label);                                                     \
    CTX_VAR->rt->Submit(CTX_VAR->self);                                        \
    _SUSPEND_END(label)

#define YIELD _YIELD_IMPL(__COUNTER__)

template <class F>
struct FMap {
    F f;
};

template <class F>
FMap(F&&) -> FMap<F>;

template <class T, class F>
auto operator|(T&& coro, FMap<F>&& f) {
    using Output = std::invoke_result_t<F, OutputOf<T>>;

    struct FMapCoro : Pc {
        FMapCoro(T&& coro, F&& f) : inner(std::move(coro)), f(std::move(f)) {
        }

        PROTO_CORO(Output) {
            PC_BEGIN;

            {
                POLL(auto res, inner);
                return f(std::move(res));
            }

            PC_END;
        }

      private:
        T inner;
        F f;
    };

    return FMapCoro{std::forward<T>(coro), std::move(f.f)};
}

template <class F>
struct AndThen {
    F f;
};

template <class T, class F>
auto operator|(T&& coro, AndThen<F>&& f) {
    using U = std::invoke_result_t<F, OutputOf<T>>;
    using Output = OutputOf<U>;

    struct AndThenCoro : Pc {
        AndThenCoro(T&& coro, F&& f) : first(std::move(coro)), f(std::move(f)) {
        }

        PROTO_CORO(Output) {
            PC_BEGIN;

            {
                POLL(auto res, first);
                new (second.template Get<U>()) U(f(std::move(*res)));
            }

            {
                POLL(auto res, *reinterpret_cast<U*>(second.Get()));
                reinterpret_cast<U*>(second.Get())->~U();
                return res;
            }

            PC_END;
        }

      private:
        T first;
        StorageFor<U> second;
        F f;
    };

    return AndThenCoro{std::forward<T>(coro), std::move(f.f)};
}

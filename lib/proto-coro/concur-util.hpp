#pragma once

#include "pc.hpp"
#include "routine.hpp"

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
struct In {};

template <class T>
constexpr inline In<T> in;

template <class T, class Runtime>
struct Spawn : IRoutine<Runtime> {
    Spawn(T&& routine, In<Runtime>) : inner(std::move(routine)) {
    }

    void Step(Runtime* rt) override {
        Context ctx{this, rt};
        inner.Step(&ctx);
    }

  private:
    T inner;
};

#define SLEEP_UNTIL(when)                                                      \
    SUSPEND_AND({ CTX_VAR->rt->After(when, CTX_VAR->self); })
#define SLEEP_FOR(duration) SLEEP_UNTIL(Clock::now() + duration)

#define WAIT_READY(fd, interest)                                               \
    SUSPEND_AND({ CTX_VAR->rt->WhenReady(fd, interest, CTX_VAR->self); })

#define YIELD SUSPEND_AND({ CTX_VAR->rt->Submit(CTX_VAR->self); })

template <class F>
struct FMap {
    F f;
};

template <class F>
FMap(F&&) -> FMap<F>;

namespace detail {

template <class T, class F>
struct FMapCoro : Pc {
    FMapCoro(T&& coro, F&& f) : inner(std::move(coro)), f(std::move(f)) {
    }

    using OutputT = std::invoke_result_t<F, OutputOf<T>>;

    PROTO_CORO(OutputT) {
        PC_BEGIN;

        {
            POLL_CORO(auto res, inner);
            return f(std::move(res));
        }

        PC_END;
    }

  private:
    T inner;
    F f;
};

}  // namespace detail

template <class T, class F>
auto operator|(T&& coro, FMap<F>&& f) {
    return detail::FMapCoro{std::forward<T>(coro), std::move(f.f)};
}

template <class F>
struct AndThen {
    F f;
};

namespace detail {

template <class T, class F>
struct AndThenCoro : Pc {
    using U = std::invoke_result_t<F, OutputOf<T>>;
    using OutputT = OutputOf<U>;

    AndThenCoro(T&& coro, F&& f) : first(std::move(coro)), f(std::move(f)) {
    }

    PROTO_CORO(OutputT) {
        PC_BEGIN;

        {
            POLL_CORO(auto res, first);
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

}  // namespace detail

template <class T, class F>
auto operator|(T&& coro, AndThen<F>&& f) {
    return detail::AndThenCoro{std::forward<T>(coro), std::move(f.f)};
}

template <class T, class Runtime>
struct SpawnDeleting final : IRoutine<Runtime> {
    SpawnDeleting(T&& routine, In<Runtime>) : inner_(std::forward<T>(routine)) {
    }

    T& GetInner() {
        return inner_;
    }

    void Step(Runtime* rt) override {
        Context ctx{this, rt};
        if (inner_.Step(&ctx).has_value()) {
            delete this;
        }
    }

  private:
    T inner_;
};

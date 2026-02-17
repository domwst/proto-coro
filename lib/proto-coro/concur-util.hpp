#pragma once

#include "pc.hpp"
#include "routine.hpp"

#include <memory>
#include <variant>

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
struct Spawn final : IRoutine<Runtime> {
    Spawn(T&& routine, In<Runtime>) : inner(std::move(routine)) {
    }

    void Step(Runtime* rt) override {
        Context ctx{this, rt};
        inner.Step(&ctx);
    }

  private:
    T inner;
};

template <class T, class Runtime>
Spawn(T&&, In<Runtime>) -> Spawn<T, Runtime>;

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

template <class F>
AndThen(F&&) -> AndThen<F>;

namespace detail {

template <class T, class F>
struct AndThenCoro : Pc {
    using U = std::invoke_result_t<F, OutputOf<T>>;
    using OutputT = OutputOf<U>;

    AndThenCoro(T&& coro, F&& f)
        : storage(StepOne{std::forward<T>(coro), std::forward<F>(f)}) {
    }

    PROTO_CORO(OutputT) {
        PC_BEGIN;

        {
            POLL_CORO(auto res, std::get_if<StepOne>(&storage)->coro);
            auto f = std::move(std::get_if<StepOne>(&storage)->cont);
            storage.template emplace<StepTwo>(f(std::move(res)));
        }

        {
            POLL_CORO(auto res, std::get_if<StepTwo>(&storage)->coro);
            return res;
        }

        PC_END;
    }

  private:
    struct StepOne {
        T coro;
        F cont;
    };

    struct StepTwo {
        U coro;
    };

    std::variant<StepOne, StepTwo> storage;
};

}  // namespace detail

template <class T, class F>
auto operator|(T&& coro, AndThen<F>&& f) {
    return detail::AndThenCoro{std::forward<T>(coro), std::move(f.f)};
}

namespace detail {

struct SelfDestructCoro : Pc {
    PROTO_CORO(Unit) {
        PC_BEGIN;

        delete CTX_VAR->self;
        return Unit{};

        PC_END;
    }
};

}  // namespace detail

constexpr inline auto SelfDestruct = [](auto&& /*prev_output*/) {
    return detail::SelfDestructCoro{};
};

template <class WaitGroup>
auto ThenDone(WaitGroup& wg) {
    return FMap{[&wg](auto&& value) {
        wg.Done();
        return std::forward<decltype(value)>(value);
    }};
};

template <class Event>
auto ThenFire(Event& ev) {
    return FMap{[&ev](auto&& value) {
        ev.Fire();
        return std::forward<decltype(value)>(value);
    }};
}

template <class T>
auto StoreResult(T& where) {
    return FMap{[&where](auto&& value) {
        where = std::forward<decltype(value)>(value);
        return Unit{};
    }};
}

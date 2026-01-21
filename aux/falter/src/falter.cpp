#include <falter/interface.hpp>

#include <macros.hpp>

#include <atomic>
#include <dlfcn.h>
#include <iostream>
#include <random>
#include <thread>

struct TrueGuard {
    TrueGuard(bool& where) : where(where) {
        where = true;
    }

    ~TrueGuard() {
        where = false;
    }

    bool& where;
};

static uint64_t FetchAdd(uint64_t& where, uint64_t val) {
    return std::atomic_ref{where}.fetch_add(val, std::memory_order_relaxed);
}

#define _FALTER_MOCK(str, ret, name, ...)                                      \
    struct str {                                                               \
        using fn_ptr = ret (*)(ARGS_MAP(ARG_TYPE, __VA_ARGS__));               \
                                                                               \
        static auto find_sym() {                                               \
            auto ptr =                                                         \
                reinterpret_cast<fn_ptr>(dlsym(RTLD_NEXT, STRINGIFY(name)));   \
            if (ptr == nullptr) {                                              \
                std::cerr << STRINGIFY(name) << " not found" << std::endl;     \
                std::abort();                                                  \
            }                                                                  \
            return ptr;                                                        \
        }                                                                      \
                                                                               \
        static fn_ptr real_##name;                                             \
        static thread_local bool in_system;                                    \
        static ret run(ARGS_MAP(ARG_DECL, __VA_ARGS__));                       \
    };                                                                         \
                                                                               \
    str::fn_ptr str::real_##name = impl_for_##name::find_sym();                \
    thread_local bool str::in_system = false;                                  \
                                                                               \
    extern "C" ret name(ARGS_MAP(ARG_DECL, __VA_ARGS__)) {                     \
        if (str::in_system) {                                                  \
            return str::real_##name(ARGS_MAP(ARG_NAME, __VA_ARGS__));          \
        }                                                                      \
        TrueGuard guard{str::in_system};                                       \
        return str::run(ARGS_MAP(ARG_NAME, __VA_ARGS__));                      \
    }                                                                          \
                                                                               \
    ret str::run(ARGS_MAP(ARG_DECL, __VA_ARGS__))

#define _IMPL_STRUCT_NAME(name) impl_for_##name

#define FALTER_MOCK(ret, name, ...)                                            \
    _FALTER_MOCK(_IMPL_STRUCT_NAME(name), ret, name, __VA_ARGS__)

#define REAL(name) _IMPL_STRUCT_NAME(name)::real_##name

static thread_local std::mt19937 rng{424243};
static thread_local int mutex_fault = 2;

static void MutexFault() {
    if (mutex_fault-- == 0) {
        std::this_thread::yield();
        mutex_fault = rng() % 9;
    }
}

FALTER_MOCK(int, pthread_mutex_lock, pthread_mutex_t*, mutex) {
    MutexFault();
    FetchAdd(GlobalStats().locks, 1);
    return real_pthread_mutex_lock(mutex);
}

FALTER_MOCK(int, pthread_mutex_timedlock, pthread_mutex_t*, mutex,
            const struct timespec*, abstime) {
    MutexFault();
    FetchAdd(GlobalStats().locks, 1);
    return real_pthread_mutex_timedlock(mutex, abstime);
}

FALTER_MOCK(int, pthread_mutex_unlock, pthread_mutex_t*, mutex) {
    MutexFault();
    return real_pthread_mutex_unlock(mutex);
}

static thread_local int cond_spurious = 3;

bool CondFault() {
    if (cond_spurious-- == 0) {
        cond_spurious = rng() % 5;
        return true;
    }
    return false;
}

FALTER_MOCK(int, pthread_cond_wait, pthread_cond_t*, cond, pthread_mutex_t*,
            mutex) {
    FetchAdd(GlobalStats().cond_waits, 1);
    if (CondFault()) {
        if (int ret = REAL(pthread_mutex_unlock)(mutex); ret != 0) {
            return ret;
        }
        std::this_thread::yield();
        REAL(pthread_mutex_lock)(mutex);
        return 0;
    }
    return real_pthread_cond_wait(cond, mutex);
}

FALTER_MOCK(int, pthread_cond_signal, pthread_cond_t*, cond) {
    FetchAdd(GlobalStats().cond_signals, 1);
    int ret = real_pthread_cond_signal(cond);
    MutexFault();
    return ret;
}

FALTER_MOCK(int, pthread_cond_broadcast, pthread_cond_t*, cond) {
    FetchAdd(GlobalStats().cond_broadcasts, 1);
    int ret = real_pthread_cond_broadcast(cond);
    MutexFault();
    return ret;
}

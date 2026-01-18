#include <falter/interface.hpp>

#include <atomic>
#include <iostream>

static uint64_t Load(const uint64_t& where) {
    return std::atomic_ref{where}.load(std::memory_order_relaxed);
}

Stats Stats::ReadImprecise() const {
    return {
        .locks = Load(locks),
        .timed_locks = Load(timed_locks),
        .cond_waits = Load(cond_waits),
        .cond_signals = Load(cond_signals),
        .cond_broadcasts = Load(cond_broadcasts),
    };
}

static Stats stats_{
    .locks = 0,
    .timed_locks = 0,
    .cond_waits = 0,
    .cond_signals = 0,
    .cond_broadcasts = 0,
};

Stats& GlobalStats() {
    return stats_;
}

std::ostream& operator<<(std::ostream& os, const Stats& s) {
    auto stats = s.ReadImprecise();
    os << "Stats{";
    os << "locks: " << stats.locks << ", ";
    os << "timed_locks: " << stats.timed_locks << ", ";
    os << "cond_waits: " << stats.cond_waits << ", ";
    os << "cond_signals: " << stats.cond_signals << ", ";
    os << "cond_broadcasts: " << stats.cond_broadcasts << "}";
    return os;
}

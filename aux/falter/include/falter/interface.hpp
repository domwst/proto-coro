#pragma once

#include <cstdint>
#include <ostream>

struct Stats {
    uint64_t locks;
    uint64_t cond_waits;
    uint64_t cond_signals;
    uint64_t cond_broadcasts;

    Stats ReadImprecise() const;
};

Stats& GlobalStats();

std::ostream& operator<<(std::ostream& os, const Stats& stats);

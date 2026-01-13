#pragma once

#include "../rt.hpp"

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <queue>

template <class T>
struct MPSCTimerQueue {
    void Push(TimePoint when, T value) {
        std::lock_guard lk{m_};
        auto top_before = TopItem();

        queue_.push(Item{
            .when = when,
            .value = std::move(value),
        });

        if (when < top_before) {
            has_items_or_closed_.notify_one();
        }
    }

    std::optional<T> Pop() {
        std::unique_lock lk{m_};

        auto top = TopItem();
        while (Clock::now() < top && !closed_) {
            has_items_or_closed_.wait_until(lk, top);
            top = TopItem();
        }

        if (queue_.empty()) {
            return std::nullopt;
        }
        auto value = std::move(queue_.top().value);
        queue_.pop();
        return value;
    }

    void Close() {
        std::lock_guard lk{m_};
        auto old_closed = std::exchange(closed_, true);
        assert(!old_closed);

        has_items_or_closed_.notify_one();
    }

  private:
    struct Item {
        TimePoint when;
        mutable T value;

        bool operator<(const Item& other) const {
            return when < other.when;
        }
    };

    TimePoint TopItem() {
        if (queue_.empty()) {
            return TimePoint::max();
        }
        return queue_.top().when;
    }

    std::mutex m_;
    std::condition_variable has_items_or_closed_;

    bool closed_ = false;
    std::priority_queue<Item> queue_;
};

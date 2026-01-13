#pragma once

#include <proto-coro/unused.hpp>

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

template <class T>
class MPMCQueue {
  public:
    void Push(T value) {
        {
            std::lock_guard lk{m_};
            queue_.push(std::move(value));
        }
        has_items_or_closed_.notify_one();
    }

    std::optional<T> Pop() {
        std::unique_lock lk{m_};
        has_items_or_closed_.wait(lk, [this] {
            return !queue_.empty() || closed_;
        });
        if (queue_.empty()) {
            return std::nullopt;
        }
        auto value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    void Close() {
        {
            std::lock_guard lk{m_};
            auto old_closed = std::exchange(closed_, true);
            assert(!old_closed);
            UNUSED(old_closed);
        }

        has_items_or_closed_.notify_all();
    }

  private:
    std::mutex m_;
    std::condition_variable has_items_or_closed_;

    bool closed_ = false;
    std::queue<T> queue_;
};

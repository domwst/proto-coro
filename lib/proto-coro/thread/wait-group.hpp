#pragma once

#include <condition_variable>
#include <mutex>

// Currently has better interface
// https://gobyexample.com/waitgroups
struct ThreadWaitGroup {
    void Add(size_t count = 1) {
        std::lock_guard lk{m_};
        count_ += count;
    }

    void Done() {
        std::lock_guard lk{m_};
        --count_;
        if (count_ == 0) {
            cv_.notify_all();
        }
    }

    void Wait() {
        std::unique_lock lk{m_};
        cv_.wait(lk, [this] {
            return count_ == 0;
        });
    }

  private:
    std::mutex m_;
    std::condition_variable cv_;
    size_t count_ = 0;
};

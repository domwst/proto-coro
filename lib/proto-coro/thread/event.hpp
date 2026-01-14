#pragma once

#include <condition_variable>
#include <mutex>

struct ThreadOneshotEvent {
    void Fire() {
        std::lock_guard lk{m_};
        fired_ = true;
        cv_.notify_all();
    }

    void Wait() {
        std::unique_lock lk{m_};
        cv_.wait(lk, [this] {
            return fired_;
        });
    }

  private:
    bool fired_ = false;

    std::mutex m_;
    std::condition_variable cv_;
};

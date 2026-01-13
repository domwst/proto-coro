#include "pc.hpp"

#include "concur-util.hpp"
#include "event-loop/event-loop.hpp"

#include <iostream>
#include <optional>

using namespace std::chrono_literals;

struct Coro : Pc {
    PROTO_CORO(int) {
        PC_BEGIN;

        std::cout << "First step" << std::endl;
        SUSPEND;

        std::cout << "Second step " << std::endl;
        SUSPEND;

        std::cout << "Third step" << std::endl;
        return 123;

        PC_END;
    }
};

struct CoroX5 : Pc {
    size_t i = 0;
    int sum = 0;
    CALLS(Coro);

    PROTO_CORO(int) {
        PC_BEGIN;

        for (; i < 5; ++i) {
            std::cout << "Calling for the " << i << "th time" << std::endl;

            {
                CALL(auto res, Coro);
                std::cout << "Got " << res << std::endl;
                sum += res;
            }
        }

        return sum;

        PC_END;
    }
};

struct YieldSleep : Pc {
    PROTO_CORO(Unit) {
        PC_BEGIN;

        std::cout << "Started " << std::this_thread::get_id() << std::endl;

        YIELD;
        std::cout << "Yielded " << std::this_thread::get_id() << std::endl;

        SLEEP_FOR(100ms);
        std::cout << "Slept " << std::this_thread::get_id() << std::endl;

        return Unit{};

        PC_END;
    }
};

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

int main() {
    EventLoop loop{2};
    loop.Start();

    ThreadOneshotEvent event;

    auto routine = Spawn{YieldSleep{} | FMap{[&](Unit) {
                             event.Fire();
                             return Unit{};
                         }}};

    loop.Submit(&routine);
    event.Wait();

    loop.Stop();
}

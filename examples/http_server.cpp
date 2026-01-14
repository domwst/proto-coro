#include <proto-coro/event-loop/event-loop.hpp>
#include <proto-coro/event-loop/fail.hpp>
#include <proto-coro/event-loop/registered-fd.hpp>
#include <proto-coro/pc.hpp>

#include <iostream>

struct BufReader {
    BufReader(RegisteredFd fd) : fd_(std::move(fd)) {
    }

    std::optional<int> Peek(const Context* ctx) {
        if (read_to_ == filled_) {
            if (!TryRefill(ctx)) {
                return std::nullopt;
            }
        }
        if (filled_ == 0) {
            return EOF;
        }

        return buf_[read_to_];
    }

    std::optional<size_t> ReadTo(std::span<char> buf, const Context* ctx) {
        if (read_to_ == filled_) {
            if (!TryRefill(ctx)) {
                return std::nullopt;
            }
        }

        auto n = std::min(buf.size(), filled_ - read_to_);
        std::copy_n(buf_ + read_to_, n, buf.begin());
        read_to_ += n;
        return n;
    }

    std::optional<int> Get(const Context* ctx) {
        char c;
        READY(auto n, ReadTo(std::span{&c, 1}, ctx));
        if (n == 0) {
            return EOF;
        }
        return c;
    }

  private:
    bool TryRefill(const Context* ctx) {
        filled_ = read_to_ = 0;

        auto r = read(fd_.AsRawFd(), buf_, sizeof(buf_));
        if (r < 0) {
            if (errno == EAGAIN) {
                ctx->rt->WhenReady(fd_.AsRawFd(), InterestKind::Readable,
                                   ctx->self);
                return false;
            }
            Fail("read");
        }
        filled_ = r;
        return true;
    }

    RegisteredFd fd_;
    char buf_[4096];
    size_t filled_ = 0;
    size_t read_to_ = 0;
};

struct ReadHeader : Pc {
    ReadHeader(BufReader& reader) : reader(reader) {
    }

    std::string buf;
    BufReader& reader;

    PROTO_CORO(std::string) {
        PC_BEGIN;

        constexpr static std::string_view suffix = "\r\n\r\n";

        while (buf.size() < suffix.size() &&
               std::string_view{buf}.substr(buf.size() - suffix.size()) !=
                   suffix) {
            READY(auto c, reader.Get(CTX_VAR));
            buf.push_back(c);
        }

        return std::move(buf);

        PC_END;
    }
};

struct Listener : Pc {
    PROTO_CORO(Unit) {
        PC_BEGIN;

        std::cout << "Listening" << std::endl;

        PC_END;
    }
};

int main() {
    EventLoop loop{2};
    loop.Start();

    loop.Stop();
}

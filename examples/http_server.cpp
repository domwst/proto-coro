#include <proto-coro/concur-util.hpp>
#include <proto-coro/event-loop/event-loop.hpp>
#include <proto-coro/event-loop/fail.hpp>
#include <proto-coro/event-loop/registered-fd.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/thread/event.hpp>

#include <netinet/ip.h>
#include <sys/socket.h>

#include <iostream>

struct BufReader {
    BufReader(RegisteredFd& fd) : fd_(fd) {
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

    RegisteredFd& fd_;
    char buf_[4096];
    size_t filled_ = 0;
    size_t read_to_ = 0;
};

struct ReadHeader : Pc {
    constexpr static std::string_view kHeaderSuffix = "\r\n\r\n";

    ReadHeader(BufReader& reader) : reader_(reader) {
    }

    PROTO_CORO(std::string) {
        PC_BEGIN;

        while (buf_.size() < kHeaderSuffix.size() ||
               std::string_view{buf_}.substr(
                   buf_.size() - kHeaderSuffix.size()) != kHeaderSuffix) {
            POLL(auto c, reader_.Get(CTX_VAR));
            if (c == EOF) {
                break;
            }
            buf_.push_back(c);
        }

        return std::move(buf_);

        PC_END;
    }

  private:
    std::string buf_;
    BufReader& reader_;
};

struct RequestServe : Pc {
    RequestServe(RegisteredFd fd) : fd_(std::move(fd)) {
    }

    void Pin() {
        reader_.emplace(fd_);
    }

    PROTO_CORO(Unit) {
        PC_BEGIN;

        {
            CALL(auto header, ReadHeader{*reader_});
            std::cout << "Received header: " << header << std::endl;
        }

        return Unit{};

        PC_END;
    }

  private:
    CALLS(ReadHeader);

    RegisteredFd fd_;
    std::optional<BufReader> reader_;
};

struct Listener : Pc {
    RegisteredFd sfd;

    Listener(RegisteredFd fd) : sfd(std::move(fd)) {
    }

    PROTO_CORO(Unit) {
        PC_BEGIN;

        std::cout << "Listening" << std::endl;

        while (true) {
            while (true) {
                struct sockaddr_in peer_addr;
                socklen_t peer_len;
                int fd = accept4(sfd.AsRawFd(), (sockaddr*)&peer_addr,
                                 &peer_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (fd == -1) {
                    if (errno == EAGAIN) {
                        break;
                    }
                    Fail("accept");
                }
                RegisteredFd rfd(OwnedFd::FromRaw(fd), CTX_VAR->rt);
                auto srv = new DeletingCoro{RequestServe(std::move(rfd))};
                srv->GetInner().Pin();
                CTX_VAR->rt->Submit(srv);
            }

            WAIT_READY(sfd.AsRawFd(), InterestKind::Readable);
        }

        PC_END;
    }
};

struct Server : Pc {
    Server(uint16_t port) : port_(port) {
    }

    void Setup(IRuntime* rt) {
        int sfd =
            socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (sfd < 0) {
            Fail("create socket");
        }

        {
            int opt = 1;
            if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
                Fail("setsockopt");
            }
        }

        sfd_.emplace(RegisteredFd{OwnedFd::FromRaw(sfd), rt});

        struct sockaddr_in sin{
            .sin_family = AF_INET,
            .sin_port = htons(port_),
            .sin_addr =
                {
                    .s_addr = 0,  // 0.0.0.0
                },
            .sin_zero = {},
        };

        int r = bind(sfd, (struct sockaddr*)&sin, sizeof(sin));
        if (r < 0) {
            Fail("bind to a port");
        }

        r = listen(sfd, 5);
        if (r < 0) {
            Fail("listen");
        }
    }

    PROTO_CORO(Unit) {
        PC_BEGIN;

        CALL_DISCARD(Listener{std::move(sfd_.value())});

        PC_END;
    }

  private:
    uint16_t port_;
    std::optional<RegisteredFd> sfd_;
    CALLS(Listener);
};

int main() {
    EventLoop loop{2};
    loop.Start();

    Server server{3333};
    server.Setup(&loop);

    ThreadOneshotEvent done;
    auto routine = Spawn{std::move(server) | FMap{[&done](Unit) {
                             done.Fire();
                             return Unit{};
                         }}};
    loop.Submit(&routine);
    done.Wait();

    loop.Stop();
}

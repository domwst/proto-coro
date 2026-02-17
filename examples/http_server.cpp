#include <proto-coro/concur-util.hpp>
#include <proto-coro/event-loop/event-loop.hpp>
#include <proto-coro/event-loop/fail.hpp>
#include <proto-coro/event-loop/registered-fd.hpp>
#include <proto-coro/pc.hpp>
#include <proto-coro/thread/event.hpp>

#include <netinet/ip.h>
#include <sys/socket.h>

#include <cassert>
#include <format>
#include <iostream>

template <class Rt>
struct BufReader {
    BufReader(RegisteredFd<Rt>& fd) : fd_(fd) {
    }

    template <class Self, class Runtime>
    std::optional<int> Peek(const Context<Self, Runtime>* ctx) {
        static_assert(std::is_same_v<Rt, Runtime>);
        READY_DISCARD(TryRefill(ctx));
        if (filled_ == 0) {
            return EOF;
        }

        return buf_[read_to_];
    }

    template <class Self, class Runtime>
    std::optional<size_t> ReadTo(std::span<char> buf,
                                 const Context<Self, Runtime>* ctx) {
        static_assert(std::is_same_v<Rt, Runtime>);
        READY_DISCARD(TryRefill(ctx));

        auto n = std::min(buf.size(), filled_ - read_to_);
        std::copy_n(buf_ + read_to_, n, buf.begin());
        read_to_ += n;
        return n;
    }

    template <class Self, class Runtime>
    std::optional<int> Get(const Context<Self, Runtime>* ctx) {
        static_assert(std::is_same_v<Rt, Runtime>);
        char c;
        READY(auto n, ReadTo(std::span{&c, 1}, ctx));
        if (n == 0) {
            return EOF;
        }
        return c;
    }

  private:
    template <class Self, class Runtime>
    std::optional<Unit> TryRefill(const Context<Self, Runtime>* ctx) {
        if (read_to_ != filled_) {
            return Unit{};
        }
        filled_ = read_to_ = 0;

        auto r = read(fd_.AsRawFd(), buf_, sizeof(buf_));
        if (r < 0) {
            if (errno == EAGAIN) {
                ctx->rt->WhenReady(fd_.AsRawFd(), InterestKind::Readable,
                                   ctx->self);
                return std::nullopt;
            }
            Fail("read");
        }
        filled_ = r;
        return Unit{};
    }

    RegisteredFd<Rt>& fd_;
    char buf_[4096];
    size_t filled_ = 0;
    size_t read_to_ = 0;
};

template <class Rt>
struct BufWriter {
    BufWriter(RegisteredFd<Rt>& fd) : fd_(fd) {
    }

    template <class Self, class Runtime>
    std::optional<size_t> Write(std::span<const char> buf,
                                const Context<Self, Runtime>* ctx) {
        static_assert(std::is_same_v<Rt, Runtime>);
        READY_DISCARD(MaybeFlush(ctx));
        auto n = std::min(buf.size(), sizeof(buf_) - filled_);
        std::copy_n(buf.begin(), n, buf_ + filled_);
        filled_ += n;
        return n;
    }

    template <class Self, class Runtime>
    std::optional<Unit> Flush(const Context<Self, Runtime>* ctx) {
        static_assert(std::is_same_v<Rt, Runtime>);
        while (written_ < filled_) {
            auto r = write(fd_.AsRawFd(), buf_ + written_, filled_ - written_);
            if (r < 0) {
                if (errno == EAGAIN) {
                    ctx->rt->WhenReady(fd_.AsRawFd(), InterestKind::Writable,
                                       ctx->self);
                    return std::nullopt;
                }
                Fail("write");
            }
            written_ += r;
        }
        written_ = filled_ = 0;
        return Unit{};
    }

    ~BufWriter() {
        assert(filled_ == written_);
    }

  private:
    template <class Self, class Runtime>
    std::optional<Unit> MaybeFlush(const Context<Self, Runtime>* ctx) {
        if (filled_ < sizeof(buf_)) {
            return Unit{};
        }
        return Flush(ctx);
    }

    RegisteredFd<Rt>& fd_;

    size_t filled_ = 0;
    size_t written_ = 0;
    char buf_[4096];
};

template <class Rt>
struct WriteAll : Pc {
    WriteAll(BufWriter<Rt>& writer, std::span<const char> buf)
        : writer_(writer), buf_(buf) {
    }

    PROTO_CORO(size_t) {
        static_assert(std::is_same_v<Rt, Runtime>);
        PC_BEGIN;

        while (!buf_.empty()) {
            POLL(auto n, writer_.Write(buf_, CTX_VAR));
            if (n == 0) {
                break;
            }
            written_ += n;
            buf_ = buf_.subspan(n);
        }

        return written_;

        PC_END;
    }

  private:
    BufWriter<Rt>& writer_;
    size_t written_ = 0;
    std::span<const char> buf_;
};

struct Response {
    uint16_t status_code;
    std::vector<std::pair<std::string, std::string>> headers;
    std::span<const char> body;
};

template <class Rt>
struct WriteResponse : Pc {
    WriteResponse(BufWriter<Rt>& writer, Response& response)
        : writer_(writer), response_(response) {
        headers_it_ = response.headers.begin();
    }

    PROTO_CORO(Unit) {
        static_assert(std::is_same_v<Rt, Runtime>);
        PC_BEGIN;

        {
            // TODO: Use a proper text version of the status code
            buf_ =
                "HTTP/1.1 " + std::to_string(response_.status_code) + " OK\r\n";
            CALL_DISCARD(WriteAll{writer_, std::span{buf_}});
        }

        while (headers_it_ < response_.headers.end()) {
            buf_ = headers_it_->first + ": " + headers_it_->second + "\r\n";

            CALL_DISCARD(WriteAll{writer_, std::span{buf_}});
            ++headers_it_;
        }
        buf_ = "Content-Length: " + std::to_string(response_.body.size()) +
               "\r\n\r\n";
        CALL_DISCARD(WriteAll{writer_, std::span{buf_}});
        CALL_DISCARD(WriteAll{writer_, response_.body});

        return Unit{};

        PC_END;
    }

  private:
    BufWriter<Rt>& writer_;
    Response& response_;
    std::vector<std::pair<std::string, std::string>>::iterator headers_it_;
    std::string buf_;
    CALLS(WriteAll<Rt>);
};

template <class Rt>
struct ReadHeader : Pc {
    constexpr static std::string_view kHeaderSuffix = "\r\n\r\n";

    ReadHeader(BufReader<Rt>& reader) : reader_(reader) {
    }

    PROTO_CORO(std::string) {
        static_assert(std::is_same_v<Rt, Runtime>);
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
    BufReader<Rt>& reader_;
};

template <class Rt>
struct RequestServe : Pc {
    RequestServe(RegisteredFd<Rt> fd) : fd_(std::move(fd)) {
    }

    PROTO_CORO(Unit) {
        PC_BEGIN;

        reader_.emplace(fd_);
        writer_.emplace(fd_);
        {
            CALL(auto header, ReadHeader{*reader_});
            std::cout << "Received header: " << header << std::endl;

            response_.status_code = 200;
            response_.headers.emplace_back("Content-Type", "text/html");
            response_.headers.emplace_back("Connection", "close");

            body_buf_ = std::format(R"(<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Title</title>
  </head>
  <body><h3>Your headers are</h3><pre>{}</pre>
  </body>
</html>
)",
                                    header);
            response_.body = body_buf_;
        }

        CALL_DISCARD(WriteResponse{*writer_, response_});
        {
            POLL_DISCARD(writer_->Flush(CTX_VAR));
        }

        return Unit{};

        PC_END;
    }

  private:
    CALLS(ReadHeader<Rt>, WriteResponse<Rt>);

    RegisteredFd<Rt> fd_;
    std::optional<BufReader<Rt>> reader_;
    std::optional<BufWriter<Rt>> writer_;
    Response response_{};
    std::string body_buf_;
};

template <class Rt>
struct Listener : Pc {
    RegisteredFd<Rt> sfd;

    Listener(RegisteredFd<Rt> fd) : sfd(std::move(fd)) {
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
                auto srv = new Spawn{RequestServe(std::move(rfd)) |
                                         AndThen{SelfDestruct},
                                     in<Runtime>};
                CTX_VAR->rt->Submit(srv);
            }

            WAIT_READY(sfd.AsRawFd(), InterestKind::Readable);
        }

        PC_END;
    }
};

template <class Rt>
struct Server : Pc {
    Server(uint16_t port) : port_(port) {
    }

    template <class Runtime>
    void Setup(Runtime* rt) {
        int sfd =
            socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (sfd < 0) {
            Fail("create socket");
        }

        {
            int opt = 1;
            if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
                -1) {
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
        return Unit{};

        PC_END;
    }

  private:
    uint16_t port_;
    std::optional<RegisteredFd<Rt>> sfd_;
    CALLS(Listener<Rt>);
};

int main() {
    EventLoop loop{2};
    loop.Start();

    Server<EventLoop> server{3333};
    server.Setup(&loop);

    ThreadOneshotEvent done;
    auto routine = Spawn{std::move(server) | ThenFire(done), in<EventLoop>};
    loop.Submit(&routine);
    done.Wait();

    loop.Stop();
}

# Proto coroutines

## Рантайм

Рантайм оперирует _рутинами_ со следующим интерфейсом:

```cpp
template <class Runtime>
struct IRoutine {
    virtual void Step(Runtime* rt) = 0;

    // Метод может использоваться для уклонения от structured concurrency.
    // С поддержкой этого метода есть проблемы, связанные с отсутствием
    // связи между состоянием корутины и сконструированными/удаленными объектами.
    // virtual void Discard() = 0;
};
```

Функциональность рантайма описывается следующими концептами:

```cpp
// После `Submit` когда-нибудь выполнит `Step` на `routine`
template <class Runtime>
concept Executor = requires(Runtime* rt, IRoutine<Runtime>* routine) {
    { rt->Submit(routine) } -> std::same_as<void>;
};

// После наступления времени `tp` выполнит `Step` на `routine`
template <class Runtime>
concept TimersManager =
    requires(Runtime* rt, IRoutine<Runtime>* routine, TimePoint tp) {
        { rt->After(tp, routine) } -> std::same_as<void>;
    };

enum class InterestKind : uint8_t {
    None = 0b00,
    Readable = 0b01,
    Writable = 0b10,
};

// Когда файловый дескриптор `fd` станет доступен на чтение/запись,
// выполнит `Step` на `routine`
template <class Runtime>
concept IOManager = requires(Runtime* rt, IRoutine<Runtime>* routine, RawFd fd,
                             InterestKind ik) {
    { rt->WhenReady(fd, ik, routine) } -> std::same_as<void>;
    // Детали реализации (epoll)
    { rt->Register(fd) } -> std::same_as<void>;
    { rt->Deregister(fd) } -> std::same_as<void>;
};
```

Создание и запуск рантайма выглядит следующим образом:

```cpp
EventLoop loop{/* num_workers= */2};
loop.Start(); // Альтернативное название - PinAndStart

// Do work

loop.Stop(); // Альтернативное название - StopAndJoin
```

## Корутины

Стековый фрейм корутины описывается пользователем в виде структуры:

```cpp
struct SteppingCoro {
    std::optional<int> Step() {
        switch (state) {
        case 0:
            for (; i < 3; ++i) {
                state = 1;
                return std::nullopt;
            case 1:
            }

            return 3;

        default:
            std::abort();
        }
    }

    size_t i = 0;
    uint16_t state = 0;
};
```

Для того, чтобы корутина могла запланировать своё пробуждение, в `Step` передаётся
_контекст_:

```cpp
template <class Self, class Runtime>
struct Context {
    Self* self;
    Runtime* rt;
};

struct SleepingCoro {
    template <class Self, class Runtime>
    std::optional<Unit> Step(Context<Self, Runtime>* ctx) {
        switch (state) {
        case 0:
            state = 1;
            ctx->rt->After(Clock::now() + std::chrono::seconds{1}, ctx->self);
            return std::nullopt;

        case 1:
            return Unit{};

        default:
            std::abort();
        }
    }

    size_t i = 0;
    uint16_t state = 0;
};
```

Если использовать нездоровое количество макросов, получается что-то похожее на
stackless-корутины:

```cpp
struct SleepYield : Pc {
    PROTO_CORO(Unit) {
        PC_BEGIN;

        SLEEP_FOR(100ms);
        YIELD;
        return Unit{};

        PC_END;
    }
};

struct CallsTwice : Pc {
    PROTO_CORO(Unit) {
        PC_BEGIN;

        CALL_DISCARD(SleepYield{});
        CALL(auto result, SleepYield{});

        return result;

        PC_END;
    }

    CALLS(SleepYield);
};
```

В `IRoutine` корутина оборачивается с помощью адаптера:

```cpp
template <class T, class Runtime>
struct Spawn final : IRoutine<Runtime> {
    Spawn(T&& routine, In<Runtime>) : inner(std::move(routine)) {
    }

    void Step(Runtime* rt) override {
        Context ctx{this, rt};
        inner.Step(&ctx);
    }

  private:
    T inner;
};
```

Благодаря тому, что в функции `Step` корутины известен конкретный тип `Runtime`,
при использовании неподдерживаемого функционала возникает ошибка компиляции.

## Комбинаторы

Так как реализация корутины довольно громоздкая, для последовательной композиции
корутин могут использоваться функциональные комбинаторы `fmap` и `>>=`
в форме `FMap` и `AndThen`, которые поддерживают pipe-based синтаксис [пример](https://github.com/domwst/proto-coro/blob/72836a97e3f22e58ddbb8e443436f9f65d8401f4/tests/real/test-sleep-yield.cpp#L51).

Для связывания корутин в рантайме с обычным кодом используются блокирующие примитивы
синхронизации:

- [Event](https://github.com/domwst/proto-coro/blob/860c74958a8a9cef8305a2e727042a46ab23faac/tests/real/test-sleep-yield.cpp#L39)
- [WaitGroup](https://github.com/domwst/proto-coro/blob/860c74958a8a9cef8305a2e727042a46ab23faac/tests/sim/load-balancing.cpp#L31)
  (используется в основном в тестах)

Возможно, явно упоминаться в задаче они не будут в пользу функции вроде

```cpp
template<class Coro, class Runtime>
decltype(auto) RunBlocking(Coro&& coro, Runtime& rt);
```

## Примеры

- [Простой пример](https://github.com/domwst/proto-coro/blob/860c74958a8a9cef8305a2e727042a46ab23faac/examples/proto_coro_demo.cpp#L71)
- [Простой некорректный HTTP-сервер](https://github.com/domwst/proto-coro/blob/860c74958a8a9cef8305a2e727042a46ab23faac/examples/http_server.cpp)

## Дополнительные особенности

<p align="right">
    Yeah, but your scientists were so<br>
    preoccupied with whether or not they could,<br>
    they didn't stop to think if they should.
</p>

Благодаря тому, что `IRoutine` знает конкретный тип `Runtime`, можно реализовать
кастомизацию структуры `IRoutine` под рантайм, в котором она исполняется:

```cpp
template <class Runtime>
struct IRoutine : Runtime::RoutineAux {
    virtual void Step(Runtime* rt) = 0;
};
```

В частности, `RoutineAux` может быть узлом интрузивной структуры данных.

## Стащено из

- [Protothreads](https://dunkels.com/adam/pt/)
- [`std::future::Future`](https://doc.rust-lang.org/stable/std/future/trait.Future.html)

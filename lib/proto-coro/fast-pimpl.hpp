#pragma once

/// @file userver/utils/fast_pimpl.hpp
/// @brief @copybrief utils::FastPimpl

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

inline constexpr bool kStrictMatch = true;

template <class T, std::size_t Size, std::size_t Alignment, bool Strict = false>
class FastPimpl final {
  public:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,performance-noexcept-move-constructor)
    FastPimpl(FastPimpl&& v) noexcept(noexcept(T(std::declval<T>())))
        : FastPimpl(std::move(*v)) {
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    FastPimpl(const FastPimpl& v) noexcept(
        noexcept(T(std::declval<const T&>())))
        : FastPimpl(*v) {
    }

    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
    FastPimpl& operator=(const FastPimpl& rhs) noexcept(
        noexcept(std::declval<T&>() = std::declval<const T&>())) {
        *AsHeld() = *rhs;
        return *this;
    }

    FastPimpl& operator=(FastPimpl&& rhs) noexcept(
        // NOLINTNEXTLINE(performance-noexcept-move-constructor)
        noexcept(std::declval<T&>() = std::declval<T>())) {
        *AsHeld() = std::move(*rhs);
        return *this;
    }

    template <typename... Args>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit FastPimpl(Args&&... args) noexcept(
        noexcept(T(std::declval<Args>()...))) {
        ::new (AsHeld()) T(std::forward<Args>(args)...);
    }

    T* operator->() noexcept {
        return AsHeld();
    }

    const T* operator->() const noexcept {
        return AsHeld();
    }

    T& operator*() noexcept {
        return *AsHeld();
    }

    const T& operator*() const noexcept {
        return *AsHeld();
    }

    ~FastPimpl() noexcept {
        Validate<sizeof(T), alignof(T), noexcept(std::declval<T*>()->~T())>();
        std::destroy_at(AsHeld());
    }

  private:
    template <std::size_t ActualSize, std::size_t ActualAlignment,
              bool ActualNoexcept>
    static void Validate() noexcept {
        static_assert(Size >= ActualSize,
                      "invalid Size: Size >= sizeof(T) failed");
        static_assert(!Strict || Size == ActualSize,
                      "invalid Size: Size == sizeof(T) failed");

        static_assert(Alignment % ActualAlignment == 0,
                      "invalid Alignment: Alignment % alignof(T) == 0 failed");
        static_assert(!Strict || Alignment == ActualAlignment,
                      "invalid Alignment: Alignment == alignof(T) failed");

        static_assert(
            ActualNoexcept,
            "Destructor of FastPimpl is marked as noexcept, the ~T() is not");
    }

    alignas(Alignment) std::byte storage_[Size];

    T* AsHeld() noexcept {
        return reinterpret_cast<T*>(&storage_);
    }

    const T* AsHeld() const noexcept {
        return reinterpret_cast<const T*>(&storage_);
    }
};

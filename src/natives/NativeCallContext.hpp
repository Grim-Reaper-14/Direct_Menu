#pragma once

#include "natives/NativeInvoker.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace smf::natives {

class NativeCallContext final {
public:
    static constexpr std::size_t MaximumArguments = 40;
    static constexpr std::size_t MaximumReturnWords = 10;

    NativeCallContext() noexcept {
        Reset();
    }

    void Reset() noexcept {
        argumentCount_ = 0;
        returnWords_.fill(0);
        arguments_.fill(0);
    }

    template <typename T>
    void Push(T&& value) {
        using Value = std::remove_cvref_t<T>;
        static_assert(
            std::is_integral_v<Value> || std::is_enum_v<Value> ||
            std::is_pointer_v<Value> || std::is_same_v<Value, float> ||
            std::is_same_v<Value, double>,
            "Unsupported native argument type.");

        if (argumentCount_ >= arguments_.size()) {
            throw std::length_error("Native argument stack capacity exceeded.");
        }

        arguments_[argumentCount_++] = Pack(static_cast<Value>(value));
    }

    template <typename T>
    [[nodiscard]] T Result() const noexcept {
        static_assert(
            std::is_integral_v<T> || std::is_enum_v<T> ||
            std::is_pointer_v<T> || std::is_same_v<T, float> ||
            std::is_same_v<T, double>,
            "Unsupported native result type.");
        return Unpack<T>(returnWords_.front());
    }

    [[nodiscard]] std::span<const NativeWord> Arguments() const noexcept {
        return {arguments_.data(), argumentCount_};
    }

    [[nodiscard]] NativeWord* ReturnStorage() noexcept {
        return returnWords_.data();
    }

    [[nodiscard]] const NativeWord* ReturnStorage() const noexcept {
        return returnWords_.data();
    }

    [[nodiscard]] std::size_t ArgumentCount() const noexcept {
        return argumentCount_;
    }

private:
    template <typename T>
    static NativeWord Pack(const T value) noexcept {
        if constexpr (std::is_pointer_v<T>) {
            return static_cast<NativeWord>(reinterpret_cast<std::uintptr_t>(value));
        } else if constexpr (std::is_same_v<T, float>) {
            return static_cast<NativeWord>(std::bit_cast<std::uint32_t>(value));
        } else if constexpr (std::is_same_v<T, double>) {
            return std::bit_cast<std::uint64_t>(value);
        } else {
            return static_cast<NativeWord>(value);
        }
    }

    template <typename T>
    static T Unpack(const NativeWord value) noexcept {
        if constexpr (std::is_pointer_v<T>) {
            return reinterpret_cast<T>(static_cast<std::uintptr_t>(value));
        } else if constexpr (std::is_same_v<T, float>) {
            return std::bit_cast<float>(static_cast<std::uint32_t>(value));
        } else if constexpr (std::is_same_v<T, double>) {
            return std::bit_cast<double>(value);
        } else {
            return static_cast<T>(value);
        }
    }

    std::array<NativeWord, MaximumReturnWords> returnWords_{};
    std::array<NativeWord, MaximumArguments> arguments_{};
    std::size_t argumentCount_{};
};

using NativeHandler = void (*)(NativeCallContext& context);

} // namespace smf::natives

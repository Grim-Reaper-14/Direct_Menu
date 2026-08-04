#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace smf::natives {

using NativeHash = std::uint64_t;
using NativeWord = std::uint64_t;

class NativeInvoker final {
public:
    enum class Environment {
        Unknown,
        SinglePlayer,
        PrivateAuthorized,
        PublicOnline
    };

    using Backend = std::function<bool(
        NativeHash hash,
        std::span<const NativeWord> arguments,
        NativeWord& result,
        std::string& errorMessage)>;

    using EnvironmentProbe = std::function<Environment()>;

    void SetBackend(Backend backend);
    void SetEnvironmentProbe(EnvironmentProbe probe);

    [[nodiscard]] bool HasBackend() const noexcept;
    [[nodiscard]] Environment CurrentEnvironment() const noexcept;
    [[nodiscard]] bool IsInvocationAllowed() const noexcept;

    template <typename Result, typename... Args>
    [[nodiscard]] std::optional<Result> Invoke(
        const NativeHash hash,
        std::string& errorMessage,
        Args... arguments) const {
        static_assert(!std::is_void_v<Result>);
        static_assert((IsSupportedArgument<Args>() && ...));
        static_assert(IsSupportedResult<Result>());

        const std::vector<NativeWord> packedArguments{
            PackArgument(arguments)...
        };

        NativeWord rawResult = 0;
        if (!InvokeRaw(hash, packedArguments, rawResult, errorMessage)) {
            return std::nullopt;
        }

        return UnpackResult<Result>(rawResult);
    }

    template <typename... Args>
    [[nodiscard]] bool InvokeVoid(
        const NativeHash hash,
        std::string& errorMessage,
        Args... arguments) const {
        static_assert((IsSupportedArgument<Args>() && ...));
        const std::vector<NativeWord> packedArguments{
            PackArgument(arguments)...
        };
        return InvokeVoidRaw(hash, packedArguments, errorMessage);
    }

    [[nodiscard]] bool InvokeVoidRaw(
        NativeHash hash,
        std::span<const NativeWord> arguments,
        std::string& errorMessage) const;

private:
    template <typename T>
    static consteval bool IsSupportedArgument() {
        return std::is_integral_v<T> || std::is_enum_v<T> ||
               std::is_pointer_v<T> || std::is_same_v<T, float> ||
               std::is_same_v<T, double>;
    }

    template <typename T>
    static consteval bool IsSupportedResult() {
        return IsSupportedArgument<T>();
    }

    template <typename T>
    static NativeWord PackArgument(const T value) noexcept {
        if constexpr (std::is_pointer_v<T>) {
            return static_cast<NativeWord>(reinterpret_cast<std::uintptr_t>(value));
        } else if constexpr (std::is_same_v<T, float>) {
            return std::bit_cast<std::uint32_t>(value);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::bit_cast<std::uint64_t>(value);
        } else {
            return static_cast<NativeWord>(value);
        }
    }

    template <typename T>
    static T UnpackResult(const NativeWord value) noexcept {
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

    [[nodiscard]] bool InvokeRaw(
        NativeHash hash,
        std::span<const NativeWord> arguments,
        NativeWord& result,
        std::string& errorMessage) const;

    mutable std::mutex mutex_;
    Backend backend_;
    EnvironmentProbe environmentProbe_;
};

void BindGeneratedNativeRuntime(
    NativeInvoker& invoker,
    std::span<const NativeHash> hashes) noexcept;
void UnbindGeneratedNativeRuntime() noexcept;
[[nodiscard]] NativeInvoker* GeneratedNativeInvoker() noexcept;
[[nodiscard]] NativeHash GeneratedNativeHash(std::size_t index) noexcept;
[[nodiscard]] const std::string& GeneratedNativeLastError() noexcept;
void SetGeneratedNativeLastError(std::string errorMessage);

} // namespace smf::natives

namespace YimMenu {

class NativeInvoker final {
public:
    template <std::size_t Index, typename Result, bool FixVectors, typename... Args>
    static Result Invoke(Args... arguments) {
        (void)FixVectors;

        auto* invoker = smf::natives::GeneratedNativeInvoker();
        if (invoker == nullptr) {
            smf::natives::SetGeneratedNativeLastError(
                "Generated native runtime is not bound to a NativeInvoker.");
            if constexpr (!std::is_void_v<Result>) {
                return Result{};
            } else {
                return;
            }
        }

        const auto hash = smf::natives::GeneratedNativeHash(Index);
        if (hash == 0) {
            smf::natives::SetGeneratedNativeLastError(
                "Generated native index is outside the loaded crossmap.");
            if constexpr (!std::is_void_v<Result>) {
                return Result{};
            } else {
                return;
            }
        }

        std::string errorMessage;
        if constexpr (std::is_void_v<Result>) {
            if (!invoker->InvokeVoid(hash, errorMessage, arguments...)) {
                smf::natives::SetGeneratedNativeLastError(std::move(errorMessage));
            } else {
                smf::natives::SetGeneratedNativeLastError({});
            }
            return;
        } else {
            const auto result = invoker->Invoke<Result>(
                hash,
                errorMessage,
                arguments...);
            if (!result) {
                smf::natives::SetGeneratedNativeLastError(std::move(errorMessage));
                return Result{};
            }

            smf::natives::SetGeneratedNativeLastError({});
            return *result;
        }
    }
};

} // namespace YimMenu

#pragma once

#include "natives/NativeCallContext.hpp"
#include "natives/NativeInvoker.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

namespace smf::natives {

class CustomNativeBackend final {
public:
    using Handler = std::function<void(NativeCallContext&)>;
    using Resolver = std::function<std::optional<Handler>(NativeHash)>;

    struct Statistics {
        std::uint64_t totalCalls{};
        std::uint64_t successfulCalls{};
        std::uint64_t failedCalls{};
        std::uint64_t rejectedThreadCalls{};
        std::uint64_t unresolvedCalls{};
    };

    CustomNativeBackend() = default;
    ~CustomNativeBackend() = default;

    CustomNativeBackend(const CustomNativeBackend&) = delete;
    CustomNativeBackend& operator=(const CustomNativeBackend&) = delete;
    CustomNativeBackend(CustomNativeBackend&&) = delete;
    CustomNativeBackend& operator=(CustomNativeBackend&&) = delete;

    bool Register(NativeHash hash, Handler handler, bool replaceExisting = true);
    bool Remove(NativeHash hash);
    void Clear() noexcept;

    void SetResolver(Resolver resolver);
    void ClearResolver();

    void BindToCurrentThread() noexcept;
    void ClearThreadBinding() noexcept;
    [[nodiscard]] bool IsCurrentThreadAllowed() const noexcept;

    [[nodiscard]] NativeInvoker::Backend CreateInvokerBackend();

    [[nodiscard]] bool Invoke(
        NativeHash hash,
        std::span<const NativeWord> arguments,
        NativeWord& result,
        std::string& errorMessage) noexcept;

    [[nodiscard]] bool Contains(NativeHash hash) const;
    [[nodiscard]] std::size_t Size() const noexcept;

    [[nodiscard]] Statistics Stats() const noexcept;
    void ResetStatistics() noexcept;

private:
    [[nodiscard]] std::optional<Handler> ResolveHandler(NativeHash hash) const;

    mutable std::mutex mutex_;
    std::unordered_map<NativeHash, Handler> handlers_;
    Resolver resolver_;
    std::optional<std::thread::id> boundThread_;

    std::atomic<std::uint64_t> totalCalls_{0};
    std::atomic<std::uint64_t> successfulCalls_{0};
    std::atomic<std::uint64_t> failedCalls_{0};
    std::atomic<std::uint64_t> rejectedThreadCalls_{0};
    std::atomic<std::uint64_t> unresolvedCalls_{0};
};

} // namespace smf::natives

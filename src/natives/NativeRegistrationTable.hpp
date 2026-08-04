#pragma once

#include "natives/NativeInvoker.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

namespace smf::natives {

class NativeRegistrationTable final {
public:
    using EngineHandler = void (*)(void* context);
    using Resolver = std::function<std::optional<EngineHandler>(
        std::uintptr_t tableAddress,
        NativeHash hash)>;

    NativeRegistrationTable() = default;
    explicit NativeRegistrationTable(std::uintptr_t tableAddress) noexcept;

    void SetAddress(std::uintptr_t tableAddress) noexcept;
    [[nodiscard]] std::uintptr_t Address() const noexcept;
    [[nodiscard]] bool IsReady() const noexcept;

    void SetResolver(Resolver resolver);
    void ClearResolver();

    [[nodiscard]] std::optional<EngineHandler> Resolve(NativeHash hash) const noexcept;
    void ClearCache() noexcept;
    [[nodiscard]] std::size_t CachedHandlerCount() const noexcept;

    [[nodiscard]] NativeInvoker::Backend CreateBackend();

    [[nodiscard]] bool Invoke(
        NativeHash hash,
        std::span<const NativeWord> arguments,
        NativeWord& result,
        std::string& errorMessage) const noexcept;

private:
    struct EngineCallContext {
        std::uint64_t* returnValue{};
        std::uint32_t argumentCount{};
        std::uint64_t* arguments{};
        std::uint32_t dataCount{};
        std::array<std::uint64_t, 96> vectorSpace{};

        void FixVectors() noexcept;
    };

    mutable std::mutex mutex_;
    std::uintptr_t tableAddress_{};
    Resolver resolver_;
    mutable std::unordered_map<NativeHash, EngineHandler> cache_;
};

} // namespace smf::natives

#include "natives/NativeRegistrationTable.inl"

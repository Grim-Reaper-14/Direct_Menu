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
    using InitNativeTables = void (*)(void* scriptProgram);

    NativeRegistrationTable() = default;
    explicit NativeRegistrationTable(std::uintptr_t tableAddress) noexcept;

    void SetAddress(std::uintptr_t tableAddress) noexcept;
    [[nodiscard]] std::uintptr_t Address() const noexcept;
    [[nodiscard]] bool IsReady() const noexcept;

    void SetResolver(Resolver resolver);
    void ClearResolver();

    // GTA V Enhanced resolves native hashes by passing a synthetic scrProgram
    // through the game's InitNativeTables routine. The address supplied through
    // SetAddress is therefore the address of InitNativeTables, not a legacy
    // linked-list registration table.
    void UseEnhancedResolver();

    [[nodiscard]] static std::optional<EngineHandler> FindNativeHandler(
        std::uintptr_t initNativeTablesAddress,
        NativeHash hash) noexcept;

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

    struct EnhancedScriptProgram {
        std::array<std::byte, 0x2C> prefix{};
        std::uint32_t nativeCount{};       // 0x2C
        std::array<std::byte, 0x10> gap{}; // 0x30-0x3F
        EngineHandler* nativeEntrypoints{}; // 0x40
        std::array<std::byte, 0x38> tail{};
    };

    static_assert(offsetof(EnhancedScriptProgram, nativeCount) == 0x2C);
    static_assert(offsetof(EnhancedScriptProgram, nativeEntrypoints) == 0x40);
    static_assert(sizeof(EnhancedScriptProgram) == 0x80);

    mutable std::mutex mutex_;
    std::uintptr_t tableAddress_{};
    Resolver resolver_;
    mutable std::unordered_map<NativeHash, EngineHandler> cache_;
};

} // namespace smf::natives

#include "natives/NativeRegistrationTable.inl"

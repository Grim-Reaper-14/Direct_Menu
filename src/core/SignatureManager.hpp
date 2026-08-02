#pragma once

#include "core/MemoryManagerAPI.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smf::core {

class SignatureManager final {
public:
    struct SignatureDefinition {
        std::string name;
        std::wstring moduleName;
        std::string pattern;
        MemoryManagerAPI::PatternScanOptions options{};
        bool resolveRelative{};
        std::size_t displacementOffset{};
        std::size_t instructionSize{};
        std::size_t dereferenceCount{};
    };

    struct ScanResult {
        std::string name;
        std::uintptr_t address{};
        bool found{};
    };

    explicit SignatureManager(MemoryManagerAPI& memoryManager) noexcept;

    bool Register(SignatureDefinition definition);
    bool Remove(std::string_view name);
    void ClearDefinitions();

    [[nodiscard]] bool Contains(std::string_view name) const;
    [[nodiscard]] std::size_t DefinitionCount() const noexcept;

    [[nodiscard]] std::optional<std::uintptr_t> Resolve(
        std::string_view name,
        bool forceRescan = false);

    [[nodiscard]] std::vector<ScanResult> ResolveAll(bool forceRescan = false);

    [[nodiscard]] std::optional<std::uintptr_t> Cached(
        std::string_view name) const;

    [[nodiscard]] bool IsCached(std::string_view name) const;
    bool Invalidate(std::string_view name);
    void InvalidateAll() noexcept;

private:
    [[nodiscard]] std::optional<std::uintptr_t> Scan(
        const SignatureDefinition& definition) const;

    MemoryManagerAPI& memoryManager_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SignatureDefinition> definitions_;
    std::unordered_map<std::string, std::uintptr_t> cache_;
};

} // namespace smf::core

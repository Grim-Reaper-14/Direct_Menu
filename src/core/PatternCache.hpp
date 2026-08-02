#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace smf::core {

class PatternCache final {
public:
    struct Entry {
        std::uintptr_t relativeAddress{};
        std::string fingerprint;
    };

    bool Load(const std::filesystem::path& path);
    bool Save(const std::filesystem::path& path) const;

    [[nodiscard]] std::optional<std::uintptr_t> Find(
        std::string_view name,
        std::string_view fingerprint,
        std::uintptr_t moduleBase) const;

    void Store(
        std::string name,
        std::string fingerprint,
        std::uintptr_t absoluteAddress,
        std::uintptr_t moduleBase);

    bool Remove(std::string_view name);
    void Clear() noexcept;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace smf::core

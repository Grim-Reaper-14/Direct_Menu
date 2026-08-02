#include "core/PatternCache.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace smf::core {

bool PatternCache::Load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::unordered_map<std::string, Entry> loaded;
    std::string name;
    std::string fingerprint;
    std::string relativeAddressText;

    while (std::getline(input, name)) {
        if (!std::getline(input, fingerprint) ||
            !std::getline(input, relativeAddressText)) {
            return false;
        }

        try {
            const auto relativeAddress = static_cast<std::uintptr_t>(
                std::stoull(relativeAddressText, nullptr, 16));
            loaded.insert_or_assign(
                name,
                Entry{
                    .relativeAddress = relativeAddress,
                    .fingerprint = fingerprint
                });
        } catch (...) {
            return false;
        }
    }

    entries_ = std::move(loaded);
    return true;
}

bool PatternCache::Save(const std::filesystem::path& path) const {
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    const auto temporaryPath = path.string() + ".tmp";
    std::ofstream output(temporaryPath, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    for (const auto& [name, entry] : entries_) {
        output << name << '\n'
               << entry.fingerprint << '\n'
               << std::hex << std::uppercase
               << entry.relativeAddress << std::dec << '\n';
    }

    output.flush();
    if (!output.good()) {
        return false;
    }
    output.close();

    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporaryPath, path, error);
    return !error;
}

std::optional<std::uintptr_t> PatternCache::Find(
    const std::string_view name,
    const std::string_view fingerprint,
    const std::uintptr_t moduleBase) const {
    if (moduleBase == 0) {
        return std::nullopt;
    }

    const auto iterator = entries_.find(std::string{name});
    if (iterator == entries_.end() ||
        iterator->second.fingerprint != fingerprint) {
        return std::nullopt;
    }

    if (iterator->second.relativeAddress > UINTPTR_MAX - moduleBase) {
        return std::nullopt;
    }

    return moduleBase + iterator->second.relativeAddress;
}

void PatternCache::Store(
    std::string name,
    std::string fingerprint,
    const std::uintptr_t absoluteAddress,
    const std::uintptr_t moduleBase) {
    if (name.empty() || fingerprint.empty() ||
        absoluteAddress < moduleBase || moduleBase == 0) {
        return;
    }

    entries_.insert_or_assign(
        std::move(name),
        Entry{
            .relativeAddress = absoluteAddress - moduleBase,
            .fingerprint = std::move(fingerprint)
        });
}

bool PatternCache::Remove(const std::string_view name) {
    return entries_.erase(std::string{name}) != 0;
}

void PatternCache::Clear() noexcept {
    entries_.clear();
}

std::size_t PatternCache::Size() const noexcept {
    return entries_.size();
}

} // namespace smf::core

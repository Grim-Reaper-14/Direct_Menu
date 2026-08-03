#include "natives/NativeCrossmap.hpp"

#include <algorithm>

namespace smf::natives {

bool NativeCrossmap::SetVersion(std::string version) {
    if (version.empty()) {
        return false;
    }

    std::scoped_lock lock(mutex_);
    version_ = std::move(version);
    return true;
}

std::string NativeCrossmap::Version() const {
    std::scoped_lock lock(mutex_);
    return version_;
}

bool NativeCrossmap::Register(
    const NativeHash originalHash,
    const NativeHash currentHash) {
    if (originalHash == 0 || currentHash == 0) {
        return false;
    }

    std::scoped_lock lock(mutex_);
    return mappings_.emplace(originalHash, currentHash).second;
}

std::size_t NativeCrossmap::RegisterMany(
    const std::span<const Mapping> mappings,
    const bool replaceExisting) {
    std::size_t registered = 0;
    std::scoped_lock lock(mutex_);

    for (const auto& [originalHash, currentHash] : mappings) {
        if (originalHash == 0 || currentHash == 0) {
            continue;
        }

        if (replaceExisting) {
            mappings_[originalHash] = currentHash;
            ++registered;
        } else if (mappings_.emplace(originalHash, currentHash).second) {
            ++registered;
        }
    }

    return registered;
}

bool NativeCrossmap::Remove(const NativeHash originalHash) {
    std::scoped_lock lock(mutex_);
    return mappings_.erase(originalHash) != 0;
}

void NativeCrossmap::Clear() noexcept {
    std::scoped_lock lock(mutex_);
    mappings_.clear();
    version_.clear();
}

bool NativeCrossmap::Contains(const NativeHash originalHash) const {
    std::scoped_lock lock(mutex_);
    return mappings_.contains(originalHash);
}

std::optional<NativeHash> NativeCrossmap::Resolve(
    const NativeHash originalHash) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = mappings_.find(originalHash);
    if (iterator == mappings_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

NativeHash NativeCrossmap::ResolveOrOriginal(
    const NativeHash originalHash) const noexcept {
    std::scoped_lock lock(mutex_);
    const auto iterator = mappings_.find(originalHash);
    return iterator == mappings_.end() ? originalHash : iterator->second;
}

std::size_t NativeCrossmap::Size() const noexcept {
    std::scoped_lock lock(mutex_);
    return mappings_.size();
}

std::vector<NativeCrossmap::Mapping> NativeCrossmap::Snapshot() const {
    std::vector<Mapping> snapshot;
    {
        std::scoped_lock lock(mutex_);
        snapshot.reserve(mappings_.size());
        for (const auto& mapping : mappings_) {
            snapshot.push_back(mapping);
        }
    }

    std::ranges::sort(
        snapshot,
        {},
        [](const Mapping& mapping) { return mapping.first; });
    return snapshot;
}

} // namespace smf::natives

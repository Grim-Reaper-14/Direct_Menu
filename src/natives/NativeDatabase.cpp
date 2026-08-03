#include "natives/NativeDatabase.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace smf::natives {

bool NativeDatabase::Register(
    NativeInfo nativeInfo,
    const bool replaceExisting) {
    if (nativeInfo.hash == 0 || nativeInfo.name.empty()) {
        return false;
    }

    const std::string normalizedName = Normalize(nativeInfo.name);
    if (normalizedName.empty()) {
        return false;
    }

    std::scoped_lock lock(mutex_);

    const auto existingByHash = entries_.find(nativeInfo.hash);
    const auto existingByName = names_.find(normalizedName);

    if (!replaceExisting &&
        (existingByHash != entries_.end() || existingByName != names_.end())) {
        return false;
    }

    if (existingByHash != entries_.end()) {
        names_.erase(Normalize(existingByHash->second.name));
    }

    if (existingByName != names_.end() && existingByName->second != nativeInfo.hash) {
        entries_.erase(existingByName->second);
    }

    names_[normalizedName] = nativeInfo.hash;
    entries_[nativeInfo.hash] = std::move(nativeInfo);
    return true;
}

std::size_t NativeDatabase::RegisterMany(
    std::vector<NativeInfo> nativeInfos,
    const bool replaceExisting) {
    std::size_t registered = 0;
    for (auto& nativeInfo : nativeInfos) {
        if (Register(std::move(nativeInfo), replaceExisting)) {
            ++registered;
        }
    }
    return registered;
}

bool NativeDatabase::Remove(const NativeHash hash) {
    std::scoped_lock lock(mutex_);
    const auto iterator = entries_.find(hash);
    if (iterator == entries_.end()) {
        return false;
    }

    names_.erase(Normalize(iterator->second.name));
    entries_.erase(iterator);
    return true;
}

void NativeDatabase::Clear() noexcept {
    std::scoped_lock lock(mutex_);
    entries_.clear();
    names_.clear();
}

bool NativeDatabase::Contains(const NativeHash hash) const {
    std::scoped_lock lock(mutex_);
    return entries_.contains(hash);
}

std::optional<NativeInfo> NativeDatabase::Find(const NativeHash hash) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = entries_.find(hash);
    if (iterator == entries_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::optional<NativeInfo> NativeDatabase::FindByName(
    const std::string_view name) const {
    const std::string normalizedName = Normalize(name);
    std::scoped_lock lock(mutex_);

    const auto nameIterator = names_.find(normalizedName);
    if (nameIterator == names_.end()) {
        return std::nullopt;
    }

    const auto entryIterator = entries_.find(nameIterator->second);
    if (entryIterator == entries_.end()) {
        return std::nullopt;
    }

    return entryIterator->second;
}

std::vector<NativeInfo> NativeDatabase::Search(
    const std::string_view query,
    const std::size_t maximumResults) const {
    const std::string normalizedQuery = Normalize(query);
    std::vector<NativeInfo> results;

    {
        std::scoped_lock lock(mutex_);
        results.reserve(entries_.size());

        for (const auto& [hash, nativeInfo] : entries_) {
            (void)hash;
            const std::string searchable = Normalize(
                nativeInfo.name + " " + nativeInfo.nameSpace + " " +
                nativeInfo.category + " " + nativeInfo.description);

            if (normalizedQuery.empty() ||
                searchable.find(normalizedQuery) != std::string::npos) {
                results.push_back(nativeInfo);
            }
        }
    }

    std::ranges::sort(results, {}, [](const NativeInfo& info) {
        return Normalize(info.name);
    });

    if (maximumResults != 0 && results.size() > maximumResults) {
        results.resize(maximumResults);
    }

    return results;
}

std::vector<NativeInfo> NativeDatabase::ByCategory(
    const std::string_view category) const {
    const std::string normalizedCategory = Normalize(category);
    std::vector<NativeInfo> results;

    {
        std::scoped_lock lock(mutex_);
        for (const auto& [hash, nativeInfo] : entries_) {
            (void)hash;
            if (Normalize(nativeInfo.category) == normalizedCategory) {
                results.push_back(nativeInfo);
            }
        }
    }

    std::ranges::sort(results, {}, [](const NativeInfo& info) {
        return Normalize(info.name);
    });
    return results;
}

std::vector<NativeInfo> NativeDatabase::Snapshot() const {
    std::vector<NativeInfo> snapshot;
    {
        std::scoped_lock lock(mutex_);
        snapshot.reserve(entries_.size());
        for (const auto& [hash, nativeInfo] : entries_) {
            (void)hash;
            snapshot.push_back(nativeInfo);
        }
    }

    std::ranges::sort(snapshot, {}, [](const NativeInfo& info) {
        return Normalize(info.name);
    });
    return snapshot;
}

std::size_t NativeDatabase::Size() const noexcept {
    std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::string NativeDatabase::Normalize(const std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());

    bool previousWasSpace = true;
    for (const unsigned char character : text) {
        if (std::isalnum(character) != 0 || character == '_') {
            normalized.push_back(static_cast<char>(std::tolower(character)));
            previousWasSpace = false;
        } else if (!previousWasSpace) {
            normalized.push_back(' ');
            previousWasSpace = true;
        }
    }

    while (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
    }

    return normalized;
}

} // namespace smf::natives

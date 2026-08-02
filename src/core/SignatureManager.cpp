#include "core/SignatureManager.hpp"

#include <algorithm>
#include <future>

namespace smf::core {

SignatureManager::SignatureManager(MemoryManagerAPI& memoryManager) noexcept
    : memoryManager_(memoryManager) {
}

bool SignatureManager::Register(SignatureDefinition definition) {
    if (definition.name.empty() || definition.pattern.empty()) {
        return false;
    }

    try {
        static_cast<void>(MemoryManagerAPI::ParsePattern(definition.pattern));
    } catch (...) {
        return false;
    }

    std::scoped_lock lock(mutex_);
    cache_.erase(definition.name);
    return definitions_.insert_or_assign(definition.name, std::move(definition)).second;
}

bool SignatureManager::Remove(const std::string_view name) {
    std::scoped_lock lock(mutex_);
    cache_.erase(std::string{name});
    return definitions_.erase(std::string{name}) != 0;
}

void SignatureManager::ClearDefinitions() {
    std::scoped_lock lock(mutex_);
    definitions_.clear();
    cache_.clear();
}

bool SignatureManager::Contains(const std::string_view name) const {
    std::scoped_lock lock(mutex_);
    return definitions_.contains(std::string{name});
}

std::size_t SignatureManager::DefinitionCount() const noexcept {
    std::scoped_lock lock(mutex_);
    return definitions_.size();
}

std::optional<std::uintptr_t> SignatureManager::Resolve(
    const std::string_view name,
    const bool forceRescan) {
    SignatureDefinition definition;
    {
        std::scoped_lock lock(mutex_);
        const std::string key{name};
        if (!forceRescan) {
            const auto cached = cache_.find(key);
            if (cached != cache_.end()) {
                return cached->second;
            }
        }

        const auto iterator = definitions_.find(key);
        if (iterator == definitions_.end()) {
            return std::nullopt;
        }
        definition = iterator->second;
    }

    const auto result = Scan(definition);
    if (result) {
        std::scoped_lock lock(mutex_);
        cache_[definition.name] = *result;
    }
    return result;
}

std::vector<SignatureManager::ScanResult> SignatureManager::ResolveAll(
    const bool forceRescan) {
    std::vector<std::string> names;
    {
        std::scoped_lock lock(mutex_);
        names.reserve(definitions_.size());
        for (const auto& [name, definition] : definitions_) {
            static_cast<void>(definition);
            names.push_back(name);
        }
    }

    std::vector<std::future<ScanResult>> tasks;
    tasks.reserve(names.size());
    for (const auto& name : names) {
        tasks.push_back(std::async(std::launch::async, [this, name, forceRescan] {
            const auto address = Resolve(name, forceRescan);
            return ScanResult{.name = name, .address = address.value_or(0), .found = address.has_value()};
        }));
    }

    std::vector<ScanResult> results;
    results.reserve(tasks.size());
    for (auto& task : tasks) {
        results.push_back(task.get());
    }

    std::ranges::sort(results, {}, &ScanResult::name);
    return results;
}

std::optional<std::uintptr_t> SignatureManager::Cached(
    const std::string_view name) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = cache_.find(std::string{name});
    return iterator == cache_.end() ? std::nullopt : std::optional{iterator->second};
}

bool SignatureManager::IsCached(const std::string_view name) const {
    std::scoped_lock lock(mutex_);
    return cache_.contains(std::string{name});
}

bool SignatureManager::Invalidate(const std::string_view name) {
    std::scoped_lock lock(mutex_);
    return cache_.erase(std::string{name}) != 0;
}

void SignatureManager::InvalidateAll() noexcept {
    std::scoped_lock lock(mutex_);
    cache_.clear();
}

std::optional<std::uintptr_t> SignatureManager::Scan(
    const SignatureDefinition& definition) const {
    const auto module = memoryManager_.FindModule(definition.moduleName);
    if (!module) {
        return std::nullopt;
    }

    std::optional<std::uintptr_t> address;
    if (definition.resolveRelative) {
        address = memoryManager_.FindRelativeAddress(
            *module,
            definition.pattern,
            definition.displacementOffset,
            definition.instructionSize,
            definition.options);
    } else {
        address = memoryManager_.FindPattern(
            *module,
            definition.pattern,
            definition.options);
    }

    if (!address) {
        return std::nullopt;
    }

    if (definition.dereferenceCount != 0) {
        return memoryManager_.DereferenceRemote(*address, definition.dereferenceCount);
    }

    return address;
}

} // namespace smf::core

#include "natives/NativeRegistry.hpp"

namespace smf::natives {

bool NativeRegistry::Register(const NativeHash hash, const NativeHandler handler) {
    if (hash == 0 || handler == nullptr) {
        return false;
    }

    std::scoped_lock lock(mutex_);
    return handlers_.emplace(hash, handler).second;
}

bool NativeRegistry::Remove(const NativeHash hash) {
    std::scoped_lock lock(mutex_);
    return handlers_.erase(hash) != 0;
}

void NativeRegistry::Clear() noexcept {
    std::scoped_lock lock(mutex_);
    handlers_.clear();
}

bool NativeRegistry::Contains(const NativeHash hash) const {
    std::scoped_lock lock(mutex_);
    return handlers_.contains(hash);
}

std::optional<NativeHandler> NativeRegistry::Find(const NativeHash hash) const {
    std::scoped_lock lock(mutex_);
    const auto iterator = handlers_.find(hash);
    if (iterator == handlers_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::size_t NativeRegistry::Size() const noexcept {
    std::scoped_lock lock(mutex_);
    return handlers_.size();
}

std::size_t NativeRegistry::Cache(
    const std::span<const Entry> entries,
    const bool replaceExisting) {
    std::size_t cached = 0;
    std::scoped_lock lock(mutex_);

    for (const auto& [hash, handler] : entries) {
        if (hash == 0 || handler == nullptr) {
            continue;
        }

        if (replaceExisting) {
            handlers_[hash] = handler;
            ++cached;
        } else if (handlers_.emplace(hash, handler).second) {
            ++cached;
        }
    }

    return cached;
}

bool NativeRegistry::Invoke(
    const NativeHash hash,
    NativeCallContext& context) const noexcept {
    NativeHandler handler = nullptr;
    {
        std::scoped_lock lock(mutex_);
        const auto iterator = handlers_.find(hash);
        if (iterator == handlers_.end()) {
            return false;
        }
        handler = iterator->second;
    }

    try {
        handler(context);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace smf::natives

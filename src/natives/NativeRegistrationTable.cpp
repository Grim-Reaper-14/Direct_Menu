#include "natives/NativeRegistrationTable.hpp"

#include <algorithm>
#include <exception>
#include <utility>
#include <vector>

namespace smf::natives {

NativeRegistrationTable::NativeRegistrationTable(
    const std::uintptr_t tableAddress) noexcept
    : tableAddress_(tableAddress) {}

void NativeRegistrationTable::SetAddress(
    const std::uintptr_t tableAddress) noexcept {
    std::scoped_lock lock(mutex_);
    tableAddress_ = tableAddress;
    cache_.clear();
}

std::uintptr_t NativeRegistrationTable::Address() const noexcept {
    std::scoped_lock lock(mutex_);
    return tableAddress_;
}

bool NativeRegistrationTable::IsReady() const noexcept {
    std::scoped_lock lock(mutex_);
    return tableAddress_ != 0 && static_cast<bool>(resolver_);
}

void NativeRegistrationTable::SetResolver(Resolver resolver) {
    std::scoped_lock lock(mutex_);
    resolver_ = std::move(resolver);
    cache_.clear();
}

void NativeRegistrationTable::ClearResolver() {
    std::scoped_lock lock(mutex_);
    resolver_ = nullptr;
    cache_.clear();
}

std::optional<NativeRegistrationTable::EngineHandler>
NativeRegistrationTable::Resolve(const NativeHash hash) const noexcept {
    if (hash == 0) {
        return std::nullopt;
    }

    Resolver resolver;
    std::uintptr_t address = 0;
    {
        std::scoped_lock lock(mutex_);
        const auto cached = cache_.find(hash);
        if (cached != cache_.end()) {
            return cached->second;
        }

        resolver = resolver_;
        address = tableAddress_;
    }

    if (!resolver || address == 0) {
        return std::nullopt;
    }

    try {
        const auto handler = resolver(address, hash);
        if (!handler || *handler == nullptr) {
            return std::nullopt;
        }

        std::scoped_lock lock(mutex_);
        cache_.insert_or_assign(hash, *handler);
        return *handler;
    } catch (...) {
        return std::nullopt;
    }
}

void NativeRegistrationTable::ClearCache() noexcept {
    std::scoped_lock lock(mutex_);
    cache_.clear();
}

std::size_t NativeRegistrationTable::CachedHandlerCount() const noexcept {
    std::scoped_lock lock(mutex_);
    return cache_.size();
}

NativeInvoker::Backend NativeRegistrationTable::CreateBackend() {
    return [this](
               const NativeHash hash,
               const std::span<const NativeWord> arguments,
               NativeWord& result,
               std::string& errorMessage) {
        return Invoke(hash, arguments, result, errorMessage);
    };
}

bool NativeRegistrationTable::Invoke(
    const NativeHash hash,
    const std::span<const NativeWord> arguments,
    NativeWord& result,
    std::string& errorMessage) const noexcept {
    result = 0;
    errorMessage.clear();

    if (arguments.size() > 40) {
        errorMessage = "Native call exceeds the 40-argument engine limit.";
        return false;
    }

    const auto handler = Resolve(hash);
    if (!handler) {
        errorMessage = "Native registration table could not resolve the requested hash.";
        return false;
    }

    try {
        std::array<std::uint64_t, 10> returnStorage{};
        std::array<std::uint64_t, 40> argumentStorage{};
        std::copy(arguments.begin(), arguments.end(), argumentStorage.begin());

        EngineCallContext context{};
        context.returnValue = returnStorage.data();
        context.argumentCount = static_cast<std::uint32_t>(arguments.size());
        context.arguments = argumentStorage.data();
        context.dataCount = 0;

        (*handler)(static_cast<void*>(&context));
        context.FixVectors();

        result = returnStorage.front();
        return true;
    } catch (const std::exception& exception) {
        errorMessage = std::string{"Native engine handler threw an exception: "} +
                       exception.what();
        return false;
    } catch (...) {
        errorMessage = "Native engine handler threw an unknown exception.";
        return false;
    }
}

void NativeRegistrationTable::EngineCallContext::FixVectors() noexcept {
    for (std::uint32_t index = 0; index + 1 < dataCount; index += 2) {
        const auto sourceAddress = static_cast<std::uintptr_t>(vectorSpace[index]);
        const auto destinationAddress = static_cast<std::uintptr_t>(vectorSpace[index + 1]);
        if (sourceAddress == 0 || destinationAddress == 0) {
            continue;
        }

        const auto* source = reinterpret_cast<const float*>(sourceAddress);
        auto* destination = reinterpret_cast<float*>(destinationAddress);
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
    }
}

} // namespace smf::natives

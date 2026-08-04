#include "natives/backends/CustomNativeBackend.hpp"

#include <exception>
#include <utility>

namespace smf::natives {

bool CustomNativeBackend::Register(
    const NativeHash hash,
    Handler handler,
    const bool replaceExisting) {
    if (hash == 0 || !handler) {
        return false;
    }

    std::scoped_lock lock{mutex_};
    if (replaceExisting) {
        handlers_.insert_or_assign(hash, std::move(handler));
        return true;
    }

    return handlers_.emplace(hash, std::move(handler)).second;
}

bool CustomNativeBackend::Remove(const NativeHash hash) {
    std::scoped_lock lock{mutex_};
    return handlers_.erase(hash) != 0;
}

void CustomNativeBackend::Clear() noexcept {
    std::scoped_lock lock{mutex_};
    handlers_.clear();
}

void CustomNativeBackend::SetResolver(Resolver resolver) {
    std::scoped_lock lock{mutex_};
    resolver_ = std::move(resolver);
}

void CustomNativeBackend::ClearResolver() {
    std::scoped_lock lock{mutex_};
    resolver_ = nullptr;
}

void CustomNativeBackend::BindToCurrentThread() noexcept {
    std::scoped_lock lock{mutex_};
    boundThread_ = std::this_thread::get_id();
}

void CustomNativeBackend::ClearThreadBinding() noexcept {
    std::scoped_lock lock{mutex_};
    boundThread_.reset();
}

bool CustomNativeBackend::IsCurrentThreadAllowed() const noexcept {
    std::scoped_lock lock{mutex_};
    return !boundThread_ || *boundThread_ == std::this_thread::get_id();
}

NativeInvoker::Backend CustomNativeBackend::CreateInvokerBackend() {
    return [this](
               const NativeHash hash,
               const std::span<const NativeWord> arguments,
               NativeWord& result,
               std::string& errorMessage) {
        return Invoke(hash, arguments, result, errorMessage);
    };
}

bool CustomNativeBackend::Invoke(
    const NativeHash hash,
    const std::span<const NativeWord> arguments,
    NativeWord& result,
    std::string& errorMessage) noexcept {
    ++totalCalls_;
    result = 0;
    errorMessage.clear();

    if (hash == 0) {
        ++failedCalls_;
        errorMessage = "Custom native backend received a zero hash.";
        return false;
    }

    if (!IsCurrentThreadAllowed()) {
        ++failedCalls_;
        ++rejectedThreadCalls_;
        errorMessage = "Custom native backend invocation occurred on the wrong thread.";
        return false;
    }

    const auto handler = ResolveHandler(hash);
    if (!handler) {
        ++failedCalls_;
        ++unresolvedCalls_;
        errorMessage = "Custom native backend could not resolve the requested native.";
        return false;
    }

    try {
        NativeCallContext context;
        for (const NativeWord argument : arguments) {
            context.Push(argument);
        }

        (*handler)(context);
        result = context.ReturnStorage()[0];
        ++successfulCalls_;
        return true;
    } catch (const std::exception& exception) {
        ++failedCalls_;
        errorMessage = std::string{"Custom native handler threw an exception: "} +
                       exception.what();
        return false;
    } catch (...) {
        ++failedCalls_;
        errorMessage = "Custom native handler threw an unknown exception.";
        return false;
    }
}

bool CustomNativeBackend::Contains(const NativeHash hash) const {
    std::scoped_lock lock{mutex_};
    return handlers_.contains(hash);
}

std::size_t CustomNativeBackend::Size() const noexcept {
    std::scoped_lock lock{mutex_};
    return handlers_.size();
}

CustomNativeBackend::Statistics CustomNativeBackend::Stats() const noexcept {
    return Statistics{
        .totalCalls = totalCalls_.load(),
        .successfulCalls = successfulCalls_.load(),
        .failedCalls = failedCalls_.load(),
        .rejectedThreadCalls = rejectedThreadCalls_.load(),
        .unresolvedCalls = unresolvedCalls_.load()
    };
}

void CustomNativeBackend::ResetStatistics() noexcept {
    totalCalls_ = 0;
    successfulCalls_ = 0;
    failedCalls_ = 0;
    rejectedThreadCalls_ = 0;
    unresolvedCalls_ = 0;
}

std::optional<CustomNativeBackend::Handler>
CustomNativeBackend::ResolveHandler(const NativeHash hash) const {
    Resolver resolver;
    {
        std::scoped_lock lock{mutex_};
        const auto iterator = handlers_.find(hash);
        if (iterator != handlers_.end()) {
            return iterator->second;
        }
        resolver = resolver_;
    }

    if (!resolver) {
        return std::nullopt;
    }

    try {
        return resolver(hash);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace smf::natives

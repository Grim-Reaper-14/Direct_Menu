#include "natives/NativeInvoker.hpp"

#include <utility>

namespace smf::natives {

void NativeInvoker::SetBackend(Backend backend) {
    std::scoped_lock lock(mutex_);
    backend_ = std::move(backend);
}

void NativeInvoker::SetEnvironmentProbe(EnvironmentProbe probe) {
    std::scoped_lock lock(mutex_);
    environmentProbe_ = std::move(probe);
}

bool NativeInvoker::HasBackend() const noexcept {
    std::scoped_lock lock(mutex_);
    return static_cast<bool>(backend_);
}

NativeInvoker::Environment NativeInvoker::CurrentEnvironment() const noexcept {
    EnvironmentProbe probe;
    {
        std::scoped_lock lock(mutex_);
        probe = environmentProbe_;
    }

    if (!probe) {
        return Environment::Unknown;
    }

    try {
        return probe();
    } catch (...) {
        return Environment::Unknown;
    }
}

bool NativeInvoker::IsInvocationAllowed() const noexcept {
    const Environment environment = CurrentEnvironment();
    return environment == Environment::SinglePlayer ||
           environment == Environment::PrivateAuthorized;
}

bool NativeInvoker::InvokeVoidRaw(
    const NativeHash hash,
    const std::span<const NativeWord> arguments,
    std::string& errorMessage) const {
    NativeWord ignoredResult = 0;
    return InvokeRaw(hash, arguments, ignoredResult, errorMessage);
}

bool NativeInvoker::InvokeRaw(
    const NativeHash hash,
    const std::span<const NativeWord> arguments,
    NativeWord& result,
    std::string& errorMessage) const {
    if (hash == 0) {
        errorMessage = "Native hash cannot be zero.";
        return false;
    }

    if (!IsInvocationAllowed()) {
        errorMessage =
            "Native invocation is disabled outside single-player or an explicitly authorized private environment.";
        return false;
    }

    Backend backend;
    {
        std::scoped_lock lock(mutex_);
        backend = backend_;
    }

    if (!backend) {
        errorMessage = "No native invocation backend is installed.";
        return false;
    }

    try {
        if (!backend(hash, arguments, result, errorMessage)) {
            if (errorMessage.empty()) {
                errorMessage = "The native backend rejected the invocation.";
            }
            return false;
        }
    } catch (const std::exception& exception) {
        errorMessage = std::string{"The native backend threw an exception: "} +
                       exception.what();
        return false;
    } catch (...) {
        errorMessage = "The native backend threw an unknown exception.";
        return false;
    }

    errorMessage.clear();
    return true;
}

} // namespace smf::natives

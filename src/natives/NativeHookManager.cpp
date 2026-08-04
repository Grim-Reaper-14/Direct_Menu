#include "natives/NativeHookManager.hpp"

#include <utility>

namespace smf::natives {

NativeHookManager::NativeHookManager(
    std::shared_ptr<NativeRegistry> registry,
    std::shared_ptr<NativeCrossmap> crossmap)
    : registry_(std::move(registry))
    , crossmap_(std::move(crossmap)) {}

NativeHookManager::~NativeHookManager() {
    Detach();
}

void NativeHookManager::Attach(NativeInvoker& invoker) {
    std::scoped_lock lock(mutex_);
    attachedInvoker_ = &invoker;
    invoker.SetBackend(
        [this](
            const NativeHash hash,
            const std::span<const NativeWord> arguments,
            NativeWord& result,
            std::string& errorMessage) {
            return DispatchInvocation(hash, arguments, result, errorMessage);
        });
}

void NativeHookManager::Detach() {
    NativeInvoker* invoker = nullptr;
    {
        std::scoped_lock lock(mutex_);
        invoker = attachedInvoker_;
        attachedInvoker_ = nullptr;
    }

    if (invoker != nullptr) {
        invoker->SetBackend(nullptr);
    }
}

bool NativeHookManager::Hook(
    const NativeHash originalHash,
    const NativeHandler handler) {
    if (originalHash == 0 || handler == nullptr) {
        return false;
    }

    // Resolve to the current hash so the registry key matches what
    // DispatchInvocation will look up at call time.
    NativeHash resolvedHash = originalHash;
    {
        std::scoped_lock lock(mutex_);
        if (crossmap_) {
            resolvedHash = crossmap_->ResolveOrOriginal(originalHash);
        }
    }

    if (registry_) {
        // Use Register; if the hash is already present, replace via Remove+Register.
        if (!registry_->Register(resolvedHash, handler)) {
            registry_->Remove(resolvedHash);
            registry_->Register(resolvedHash, handler);
        }
        return true;
    }

    return false;
}

bool NativeHookManager::Unhook(const NativeHash originalHash) {
    if (originalHash == 0 || !registry_) {
        return false;
    }

    NativeHash resolvedHash = originalHash;
    {
        std::scoped_lock lock(mutex_);
        if (crossmap_) {
            resolvedHash = crossmap_->ResolveOrOriginal(originalHash);
        }
    }

    return registry_->Remove(resolvedHash);
}

void NativeHookManager::UnhookAll() noexcept {
    if (registry_) {
        registry_->Clear();
    }
}

void NativeHookManager::SetFallbackBackend(NativeInvoker::Backend fallback) {
    std::scoped_lock lock(mutex_);
    fallback_ = std::move(fallback);
}

bool NativeHookManager::IsAttached() const noexcept {
    std::scoped_lock lock(mutex_);
    return attachedInvoker_ != nullptr;
}

std::size_t NativeHookManager::HookCount() const noexcept {
    if (!registry_) {
        return 0;
    }
    return registry_->Size();
}

bool NativeHookManager::DispatchInvocation(
    const NativeHash hash,
    const std::span<const NativeWord> arguments,
    NativeWord& result,
    std::string& errorMessage) const {
    // Resolve through crossmap (lock-free after copy).
    NativeHash resolvedHash = hash;
    NativeInvoker::Backend fallback;
    {
        std::scoped_lock lock(mutex_);
        if (crossmap_) {
            resolvedHash = crossmap_->ResolveOrOriginal(hash);
        }
        fallback = fallback_;
    }

    // Try the registry (does not hold our mutex during handler execution).
    if (registry_) {
        NativeCallContext context;
        for (const NativeWord argument : arguments) {
            context.Push(argument);
        }

        if (registry_->Invoke(resolvedHash, context)) {
            result = context.ReturnStorage()[0];
            errorMessage.clear();
            return true;
        }
    }

    // No hook: try fallback.
    if (fallback) {
        return fallback(resolvedHash, arguments, result, errorMessage);
    }

    errorMessage = "No hook registered for the requested native (hash 0x";
    // Append hex hash for diagnostics.
    constexpr const char* kHex = "0123456789abcdef";
    for (int shift = 60; shift >= 0; shift -= 4) {
        errorMessage.push_back(kHex[(resolvedHash >> shift) & 0xF]);
    }
    errorMessage += ").";
    return false;
}

} // namespace smf::natives

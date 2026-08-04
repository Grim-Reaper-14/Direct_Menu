#pragma once

#include "natives/NativeCallContext.hpp"
#include "natives/NativeCrossmap.hpp"
#include "natives/NativeInvoker.hpp"
#include "natives/NativeRegistry.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace smf::natives {

/// Wires a NativeRegistry and NativeCrossmap into a NativeInvoker as a
/// composable backend layer.
///
/// Lifecycle:
///   1. Construct with (or assign) a NativeRegistry and NativeCrossmap.
///   2. Call Attach(invoker) to install the backend.
///   3. Register hooks via Hook(originalHash, handler).
///   4. Call Detach() (or destroy) to remove the backend from the invoker.
///
/// Call routing:
///   - The original hash is first translated through the crossmap (if populated).
///   - If the resolved hash has a registered hook in the registry, that handler
///     is called and its return word is returned to the invoker.
///   - If no hook is found the invocation is forwarded to the fallback backend
///     (if one was set via SetFallbackBackend).
///   - If neither a hook nor a fallback exists the call fails with a clear error.
///
/// Thread safety: all public methods are thread-safe.
class NativeHookManager final {
public:
    explicit NativeHookManager(
        std::shared_ptr<NativeRegistry> registry,
        std::shared_ptr<NativeCrossmap> crossmap);

    NativeHookManager() = default;
    ~NativeHookManager();

    NativeHookManager(const NativeHookManager&) = delete;
    NativeHookManager& operator=(const NativeHookManager&) = delete;
    NativeHookManager(NativeHookManager&&) = delete;
    NativeHookManager& operator=(NativeHookManager&&) = delete;

    /// Install this manager as the backend on @p invoker.
    /// Replaces any previously installed backend.
    void Attach(NativeInvoker& invoker);

    /// Remove this manager as the backend from the invoker set in Attach().
    /// No-op if not currently attached.
    void Detach();

    /// Register a hook for a native identified by its original (stable) hash.
    /// @return false if @p originalHash is 0 or @p handler is null.
    bool Hook(NativeHash originalHash, NativeHandler handler);

    /// Remove the hook for @p originalHash.
    /// @return true if a hook was found and removed.
    bool Unhook(NativeHash originalHash);

    /// Remove all registered hooks.
    void UnhookAll() noexcept;

    /// Optionally set a fallback backend that is called when no hook is found
    /// for a resolved hash.  Pass an empty function to clear.
    void SetFallbackBackend(NativeInvoker::Backend fallback);

    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] std::size_t HookCount() const noexcept;

private:
    bool DispatchInvocation(
        NativeHash hash,
        std::span<const NativeWord> arguments,
        NativeWord& result,
        std::string& errorMessage) const;

    mutable std::mutex mutex_;
    std::shared_ptr<NativeRegistry> registry_;
    std::shared_ptr<NativeCrossmap> crossmap_;
    NativeInvoker::Backend fallback_;
    NativeInvoker* attachedInvoker_{nullptr};
};

} // namespace smf::natives

#include "sdk/SDK.hpp"

namespace smf::sdk {


SDK::SDK(
 // Load all authorized natives
    for (const auto& n : kNativeList) {
        nativeDatabase_.Register(n.name, n.hash);
    }

    core::MemoryManagerAPI& memory,
    core::SignatureManager& signatures,
    natives::NativeInvoker& nativeInvoker,
    natives::NativeRegistry& nativeRegistry,
    natives::NativeScheduler& nativeScheduler,
    natives::NativeCrossmap& nativeCrossmap,
    natives::NativeDatabase& nativeDatabase) noexcept
    : memory_(&memory),
      signatures_(&signatures),
      nativeInvoker_(&nativeInvoker),
      nativeRegistry_(&nativeRegistry),
      nativeScheduler_(&nativeScheduler),
      nativeCrossmap_(&nativeCrossmap),
      nativeDatabase_(&nativeDatabase) {
}

core::MemoryManagerAPI& SDK::Memory() noexcept { return *memory_; }
const core::MemoryManagerAPI& SDK::Memory() const noexcept { return *memory_; }

core::SignatureManager& SDK::Signatures() noexcept { return *signatures_; }
const core::SignatureManager& SDK::Signatures() const noexcept { return *signatures_; }

natives::NativeInvoker& SDK::NativeInvoker() noexcept { return *nativeInvoker_; }
const natives::NativeInvoker& SDK::NativeInvoker() const noexcept { return *nativeInvoker_; }

natives::NativeRegistry& SDK::NativeRegistry() noexcept { return *nativeRegistry_; }
const natives::NativeRegistry& SDK::NativeRegistry() const noexcept { return *nativeRegistry_; }

natives::NativeScheduler& SDK::NativeScheduler() noexcept { return *nativeScheduler_; }
const natives::NativeScheduler& SDK::NativeScheduler() const noexcept { return *nativeScheduler_; }

natives::NativeCrossmap& SDK::NativeCrossmap() noexcept { return *nativeCrossmap_; }
const natives::NativeCrossmap& SDK::NativeCrossmap() const noexcept { return *nativeCrossmap_; }

natives::NativeDatabase& SDK::NativeDatabase() noexcept { return *nativeDatabase_; }
const natives::NativeDatabase& SDK::NativeDatabase() const noexcept { return *nativeDatabase_; }

} // namespace smf::sdk

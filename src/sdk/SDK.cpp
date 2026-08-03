#include "sdk/SDK.hpp"

namespace smf::sdk {

SDK::SDK(
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
      nativeDatabase_(&nativeDatabase),
      players_(*this),
      vehicles_(*this),
      cameras_(*this),
      world_(*this) {
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

PlayerManager& SDK::Players() noexcept { return players_; }
const PlayerManager& SDK::Players() const noexcept { return players_; }

VehicleManager& SDK::Vehicles() noexcept { return vehicles_; }
const VehicleManager& SDK::Vehicles() const noexcept { return vehicles_; }

CameraManager& SDK::Cameras() noexcept { return cameras_; }
const CameraManager& SDK::Cameras() const noexcept { return cameras_; }

World& SDK::GameWorld() noexcept { return world_; }
const World& SDK::GameWorld() const noexcept { return world_; }

SDKDiagnostics SDK::Diagnostics() const {
    const auto scheduler = nativeScheduler_->Stats();

    SDKDiagnostics diagnostics{};
    diagnostics.processAttached = memory_->IsProcessAttached();
    diagnostics.signatureDefinitions = signatures_->DefinitionCount();
    diagnostics.nativeBackendAvailable = nativeInvoker_->HasBackend();
    diagnostics.nativeEnvironment = nativeInvoker_->CurrentEnvironment();
    diagnostics.nativeInvocationAllowed = nativeInvoker_->IsInvocationAllowed();
    diagnostics.registeredNatives = nativeRegistry_->Size();
    diagnostics.pendingNativeTasks = nativeScheduler_->PendingCount();
    diagnostics.schedulerFrames = scheduler.frames;
    diagnostics.executedNativeTasks = scheduler.executedTasks;
    diagnostics.failedNativeTasks = scheduler.failedTasks;
    diagnostics.crossmapEntries = nativeCrossmap_->Size();
    diagnostics.crossmapVersion = nativeCrossmap_->Version();
    diagnostics.nativeMetadataEntries = nativeDatabase_->Size();
    diagnostics.hasLocalPlayer = players_.HasLocal();
    diagnostics.localPlayerHandle = players_.LocalHandle();
    return diagnostics;
}

} // namespace smf::sdk

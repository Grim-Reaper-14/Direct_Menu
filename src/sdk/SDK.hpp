#pragma once

#include "core/MemoryManagerAPI.hpp"
#include "core/SignatureManager.hpp"
#include "natives/NativeCrossmap.hpp"
#include "natives/NativeDatabase.hpp"
#include "natives/NativeInvoker.hpp"
#include "natives/NativeRegistry.hpp"
#include "natives/NativeScheduler.hpp"
#include "sdk/CameraManager.hpp"
#include "sdk/EntityManager.hpp"
#include "sdk/PlayerManager.hpp"
#include "sdk/SDKDiagnostics.hpp"
#include "sdk/VehicleManager.hpp"
#include "sdk/World.hpp"

namespace smf::sdk {

class SDK final {
public:
    SDK(
        core::MemoryManagerAPI& memory,
        core::SignatureManager& signatures,
        natives::NativeInvoker& nativeInvoker,
        natives::NativeRegistry& nativeRegistry,
        natives::NativeScheduler& nativeScheduler,
        natives::NativeCrossmap& nativeCrossmap,
        natives::NativeDatabase& nativeDatabase) noexcept;

    SDK(const SDK&) = delete;
    SDK& operator=(const SDK&) = delete;
    SDK(SDK&&) = delete;
    SDK& operator=(SDK&&) = delete;

    [[nodiscard]] core::MemoryManagerAPI& Memory() noexcept;
    [[nodiscard]] const core::MemoryManagerAPI& Memory() const noexcept;

    [[nodiscard]] core::SignatureManager& Signatures() noexcept;
    [[nodiscard]] const core::SignatureManager& Signatures() const noexcept;

    [[nodiscard]] natives::NativeInvoker& NativeInvoker() noexcept;
    [[nodiscard]] const natives::NativeInvoker& NativeInvoker() const noexcept;

    [[nodiscard]] natives::NativeRegistry& NativeRegistry() noexcept;
    [[nodiscard]] const natives::NativeRegistry& NativeRegistry() const noexcept;

    [[nodiscard]] natives::NativeScheduler& NativeScheduler() noexcept;
    [[nodiscard]] const natives::NativeScheduler& NativeScheduler() const noexcept;

    [[nodiscard]] natives::NativeCrossmap& NativeCrossmap() noexcept;
    [[nodiscard]] const natives::NativeCrossmap& NativeCrossmap() const noexcept;

    [[nodiscard]] natives::NativeDatabase& NativeDatabase() noexcept;
    [[nodiscard]] const natives::NativeDatabase& NativeDatabase() const noexcept;

    [[nodiscard]] EntityManager& Entities() noexcept;
    [[nodiscard]] const EntityManager& Entities() const noexcept;

    [[nodiscard]] PlayerManager& Players() noexcept;
    [[nodiscard]] const PlayerManager& Players() const noexcept;

    [[nodiscard]] VehicleManager& Vehicles() noexcept;
    [[nodiscard]] const VehicleManager& Vehicles() const noexcept;

    [[nodiscard]] CameraManager& Cameras() noexcept;
    [[nodiscard]] const CameraManager& Cameras() const noexcept;

    [[nodiscard]] World& GameWorld() noexcept;
    [[nodiscard]] const World& GameWorld() const noexcept;

    [[nodiscard]] SDKDiagnostics Diagnostics() const;

private:
    core::MemoryManagerAPI* memory_{};
    core::SignatureManager* signatures_{};
    natives::NativeInvoker* nativeInvoker_{};
    natives::NativeRegistry* nativeRegistry_{};
    natives::NativeScheduler* nativeScheduler_{};
    natives::NativeCrossmap* nativeCrossmap_{};
    natives::NativeDatabase* nativeDatabase_{};
    EntityManager entities_;
    PlayerManager players_;
    VehicleManager vehicles_;
    CameraManager cameras_;
    World world_;
};

} // namespace smf::sdk

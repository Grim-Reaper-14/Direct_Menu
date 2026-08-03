#include "core/MemoryManagerAPI.hpp"
#include "core/SignatureManager.hpp"
#include "natives/NativeCrossmap.hpp"
#include "natives/NativeDatabase.hpp"
#include "natives/NativeInvoker.hpp"
#include "natives/NativeRegistry.hpp"
#include "natives/NativeScheduler.hpp"
#include "sdk/Camera.hpp"
#include "sdk/CameraManager.hpp"
#include "sdk/Entity.hpp"
#include "sdk/EntityManager.hpp"
#include "sdk/Player.hpp"
#include "sdk/PlayerManager.hpp"
#include "sdk/SDK.hpp"
#include "sdk/Types.hpp"
#include "sdk/Vehicle.hpp"
#include "sdk/VehicleManager.hpp"
#include "sdk/World.hpp"

#include <iostream>
#include <type_traits>

namespace {

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    smf::core::MemoryManagerAPI memory;
    smf::core::SignatureManager signatures(memory);
    smf::natives::NativeInvoker nativeInvoker;
    smf::natives::NativeRegistry nativeRegistry;
    smf::natives::NativeScheduler nativeScheduler;
    smf::natives::NativeCrossmap nativeCrossmap;
    smf::natives::NativeDatabase nativeDatabase;

    smf::sdk::SDK services(
        memory,
        signatures,
        nativeInvoker,
        nativeRegistry,
        nativeScheduler,
        nativeCrossmap,
        nativeDatabase);

    static_assert(!std::is_copy_constructible_v<smf::sdk::SDK>);
    static_assert(!std::is_move_constructible_v<smf::sdk::SDK>);

    const smf::sdk::SDKDiagnostics initialDiagnostics = services.Diagnostics();
    if (initialDiagnostics.processAttached ||
        initialDiagnostics.signatureDefinitions != 0 ||
        initialDiagnostics.nativeBackendAvailable ||
        initialDiagnostics.nativeEnvironment !=
            smf::natives::NativeInvoker::Environment::Unknown ||
        initialDiagnostics.nativeInvocationAllowed ||
        initialDiagnostics.registeredNatives != 0 ||
        initialDiagnostics.pendingNativeTasks != 0 ||
        initialDiagnostics.crossmapEntries != 0 ||
        initialDiagnostics.nativeMetadataEntries != 0 ||
        initialDiagnostics.hasLocalPlayer ||
        initialDiagnostics.localPlayerHandle !=
            smf::sdk::InvalidEntityHandle) {
        return Fail("Initial SDK diagnostics failed.");
    }

    const smf::sdk::Entity empty;
    if (empty.IsBound() || empty.HasValue() || empty ||
        empty.Handle() != smf::sdk::InvalidEntityHandle ||
        empty.Services() != nullptr) {
        return Fail("Default Entity state failed.");
    }

    smf::sdk::EntityManager& entities = services.Entities();
    smf::sdk::Entity entity = entities.FromHandle(42);
    if (!entity.IsBound() || !entity.HasValue() || !entity ||
        entity.Handle() != 42 || entity.Services() != &services) {
        return Fail("Bound Entity state failed.");
    }

    const smf::sdk::Entity copy = entity;
    if (copy != entity || copy.Services() != &services) {
        return Fail("Entity value copy failed.");
    }

    const smf::sdk::Entity invalid =
        entities.FromHandle(smf::sdk::InvalidEntityHandle);
    if (!invalid.IsBound() || invalid.HasValue() || invalid ||
        invalid.Services() != &services) {
        return Fail("Invalid Entity handle state failed.");
    }

    entity.Reset();
    if (entity != empty) {
        return Fail("Entity reset failed.");
    }

    smf::sdk::PlayerManager& players = services.Players();
    if (players.HasLocal() ||
        players.LocalHandle() != smf::sdk::InvalidEntityHandle) {
        return Fail("PlayerManager default local state failed.");
    }

    const smf::sdk::Player player = players.FromHandle(7);
    if (!player || player.Handle() != 7 || player.Services() != &services) {
        return Fail("PlayerManager handle construction failed.");
    }

    const smf::sdk::Player emptyLocal = players.Local();
    if (emptyLocal || !emptyLocal.IsBound() ||
        emptyLocal.Services() != &services) {
        return Fail("PlayerManager empty local wrapper failed.");
    }

    players.SetLocalHandle(11);
    const smf::sdk::Player local = players.Local();
    if (!players.HasLocal() || players.LocalHandle() != 11 ||
        !local || local.Handle() != 11 || local.Services() != &services) {
        return Fail("PlayerManager local handle state failed.");
    }

    const smf::sdk::SDKDiagnostics localDiagnostics = services.Diagnostics();
    if (!localDiagnostics.hasLocalPlayer ||
        localDiagnostics.localPlayerHandle != 11) {
        return Fail("SDK local-player diagnostics failed.");
    }

    players.ClearLocal();
    if (players.HasLocal() || players.Local()) {
        return Fail("PlayerManager local reset failed.");
    }

    smf::sdk::VehicleManager& vehicles = services.Vehicles();
    const smf::sdk::Vehicle vehicle = vehicles.FromHandle(23);
    if (!vehicle || vehicle.Handle() != 23 ||
        vehicle.Services() != &services) {
        return Fail("VehicleManager handle construction failed.");
    }

    const smf::sdk::Vehicle invalidVehicle =
        vehicles.FromHandle(smf::sdk::InvalidEntityHandle);
    if (invalidVehicle || !invalidVehicle.IsBound() ||
        invalidVehicle.Services() != &services) {
        return Fail("VehicleManager invalid handle state failed.");
    }

    smf::sdk::CameraManager& cameras = services.Cameras();
    smf::sdk::Camera camera = cameras.FromHandle(31);
    if (!camera || camera.Handle() != 31 || camera.Services() != &services) {
        return Fail("CameraManager handle construction failed.");
    }

    camera.Reset();
    if (camera || camera.IsBound() ||
        camera.Handle() != smf::sdk::InvalidCameraHandle) {
        return Fail("Camera reset failed.");
    }

    const smf::sdk::Camera invalidCamera =
        cameras.FromHandle(smf::sdk::InvalidCameraHandle);
    if (invalidCamera || !invalidCamera.IsBound() ||
        invalidCamera.Services() != &services) {
        return Fail("CameraManager invalid handle state failed.");
    }

    smf::sdk::World& world = services.GameWorld();
    if (&world.Entities() != &services.Entities() ||
        &world.Players() != &services.Players() ||
        &world.Vehicles() != &services.Vehicles() ||
        &world.Cameras() != &services.Cameras()) {
        return Fail("World manager facade failed.");
    }

    constexpr smf::sdk::Vector3 position{1.0F, 2.0F, 3.0F};
    constexpr smf::sdk::Vector3 offset{4.0F, 5.0F, 6.0F};
    constexpr smf::sdk::Vector3 expected{5.0F, 7.0F, 9.0F};
    constexpr auto translated = position + offset;
    if (translated.x != expected.x || translated.y != expected.y ||
        translated.z != expected.z) {
        return Fail("Shared SDK Vector3 integration failed.");
    }

    return 0;
}

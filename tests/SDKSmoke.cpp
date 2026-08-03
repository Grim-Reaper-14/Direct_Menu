#include "core/MemoryManagerAPI.hpp"
#include "core/SignatureManager.hpp"
#include "natives/NativeCrossmap.hpp"
#include "natives/NativeDatabase.hpp"
#include "natives/NativeInvoker.hpp"
#include "natives/NativeRegistry.hpp"
#include "natives/NativeScheduler.hpp"
#include "sdk/Entity.hpp"
#include "sdk/Player.hpp"
#include "sdk/PlayerManager.hpp"
#include "sdk/SDK.hpp"
#include "sdk/Types.hpp"

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

    const smf::sdk::Entity empty;
    if (empty.IsBound() || empty.HasValue() || empty ||
        empty.Handle() != smf::sdk::InvalidEntityHandle ||
        empty.Services() != nullptr) {
        return Fail("Default Entity state failed.");
    }

    smf::sdk::Entity entity(services, 42);
    if (!entity.IsBound() || !entity.HasValue() || !entity ||
        entity.Handle() != 42 || entity.Services() != &services) {
        return Fail("Bound Entity state failed.");
    }

    const smf::sdk::Entity copy = entity;
    if (copy != entity || copy.Services() != &services) {
        return Fail("Entity value copy failed.");
    }

    const smf::sdk::Entity invalid(services, smf::sdk::InvalidEntityHandle);
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

    players.ClearLocal();
    if (players.HasLocal() || players.Local()) {
        return Fail("PlayerManager local reset failed.");
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

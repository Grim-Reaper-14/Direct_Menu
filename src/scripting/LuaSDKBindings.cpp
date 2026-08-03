#include "scripting/LuaSDKBindings.hpp"

#include "sdk/Camera.hpp"
#include "sdk/Entity.hpp"
#include "sdk/Player.hpp"
#include "sdk/SDK.hpp"
#include "sdk/Vehicle.hpp"

#include <cstdint>

namespace smf::scripting {
namespace {

template <typename Value>
void RegisterHandleValue(sol::state& lua, const char* name) {
    auto type = lua.new_usertype<Value>(name, sol::no_constructor);
    type.set_function("is_bound", [](const Value& value) {
        return value.IsBound();
    });
    type.set_function("valid", [](const Value& value) {
        return value.HasValue();
    });
    type.set_function("handle", [](const Value& value) {
        return value.Handle();
    });
}

} // namespace

void LuaSDKBindings::Register(sol::state& lua, sdk::SDK& services) {
    RegisterHandleValue<sdk::Entity>(lua, "Entity");
    RegisterHandleValue<sdk::Player>(lua, "Player");
    RegisterHandleValue<sdk::Vehicle>(lua, "Vehicle");
    RegisterHandleValue<sdk::Camera>(lua, "Camera");

    sol::table api = lua.create_named_table("SDK");
    api.set_function("entity", [&services](const std::uint32_t handle) {
        return sdk::Entity(services, handle);
    });
    api.set_function("player", [&services](const std::uint32_t handle) {
        return services.Players().FromHandle(handle);
    });
    api.set_function("vehicle", [&services](const std::uint32_t handle) {
        return services.Vehicles().FromHandle(handle);
    });
    api.set_function("camera", [&services](const std::uint32_t handle) {
        return services.Cameras().FromHandle(handle);
    });
    api.set_function("has_local_player", [&services] {
        return services.Players().HasLocal();
    });
    api.set_function("local_player", [&services] {
        return services.Players().Local();
    });
}

} // namespace smf::scripting

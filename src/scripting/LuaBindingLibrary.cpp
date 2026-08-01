#include "scripting/LuaBindingLibrary.hpp"

#include "logging/Logger.hpp"
#include "scripting/LuaCommands.hpp"

namespace smf::scripting {

LuaBindingLibrary::LuaBindingLibrary(
    logging::LoggerApi& logger,
    LuaCommands& commands)
    : logger_(logger),
      commands_(commands) {
}

void LuaBindingLibrary::BindFeatureRegistry(features::FeatureRegistry& registry) noexcept {
    registry_ = &registry;
}

void LuaBindingLibrary::RegisterCoreBindings() {
    if (ready_) {
        return;
    }

    commands_.Register(
        "lua.refresh",
        "Reserved command entry for refreshing the Lua script list.",
        [this] {
            logger_.Debug("Lua refresh command invoked through the binding boundary.");
        });

    ready_ = true;
    logger_.Info("Lua binding library initialized.");
}

bool LuaBindingLibrary::Ready() const noexcept {
    return ready_ && registry_ != nullptr;
}

} // namespace smf::scripting

#include "scripting/LuaBindingLibrary.hpp"

#include "logging/Logger.hpp"
#include "scripting/LuaCommands.hpp"
#include "scripting/LuaEvents.hpp"
#include "scripting/LuaTimerManager.hpp"
#include "scripting/LuaUI.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace smf::scripting {

LuaBindingLibrary::LuaBindingLibrary(
    logging::LoggerApi& logger,
    sol::state& lua,
    LuaCommands& commands,
    LuaEvents& events,
    LuaTimerManager& timers,
    LuaUI& ui,
    OwnerProvider ownerProvider,
    ImGuiFrameScopeProvider frameScopeProvider,
    RefreshCallback refreshCallback)
    : logger_(logger),
      lua_(lua),
      commands_(commands),
      events_(events),
      timers_(timers),
      ui_(ui),
      ownerProvider_(std::move(ownerProvider)),
      frameScopeProvider_(std::move(frameScopeProvider)),
      refreshCallback_(std::move(refreshCallback)) {
}

void LuaBindingLibrary::BindFeatureRegistry(features::FeatureRegistry& registry) noexcept {
    registry_ = &registry;
}

void LuaBindingLibrary::RegisterCoreBindings() {
    if (ready_) {
        return;
    }

    RegisterLogging();
    RegisterCommands();
    RegisterEvents();
    RegisterTimers();
    RegisterUI();
    RegisterImGui();
    RegisterApplication();

    lua_["LUA_API_VERSION"] = "1.0.0";
    ready_ = true;
    logger_.Info("Lua binding library initialized with sol2 bindings.");
}

void LuaBindingLibrary::RegisterLogging() {
    sol::table log = lua_.create_named_table("log");
    log.set_function("debug", [this](const std::string& message) { logger_.Debug("[Lua] " + message); });
    log.set_function("info", [this](const std::string& message) { logger_.Info("[Lua] " + message); });
    log.set_function("warn", [this](const std::string& message) { logger_.Warning("[Lua] " + message); });
    log.set_function("error", [this](const std::string& message) { logger_.Error("[Lua] " + message); });
}

void LuaBindingLibrary::RegisterCommands() {
    sol::table command = lua_.create_named_table("command");
    command.set_function("exists", [this](const std::string& name) {
        return commands_.Exists(name);
    });
    command.set_function("execute", [this](const std::string& name) {
        return commands_.Execute(name);
    });
    command.set_function("list", [this]() {
        sol::table result = lua_.create_table();
        std::size_t index = 1;
        for (const LuaCommands::CommandInfo& info : commands_.List()) {
            sol::table entry = lua_.create_table();
            entry["name"] = info.name;
            entry["description"] = info.description;
            entry["owner"] = info.owner;
            result[index++] = std::move(entry);
        }
        return result;
    });
}

void LuaBindingLibrary::RegisterEvents() {
    sol::table event = lua_.create_named_table("event");
    event.set_function("on", [this](const std::string& name, sol::protected_function callback) {
        return events_.Subscribe(name, ownerProvider_(), std::move(callback));
    });
    event.set_function("off", [this](const std::uint64_t id) {
        return events_.Unsubscribe(id);
    });
    event.set_function("emit", [this](const std::string& name) {
        events_.Emit(name);
    });
}

void LuaBindingLibrary::RegisterTimers() {
    sol::table timer = lua_.create_named_table("timer");
    timer.set_function("after", [this](std::int64_t milliseconds, sol::protected_function callback) {
        return timers_.After(
            std::chrono::milliseconds{std::max<std::int64_t>(milliseconds, 0)},
            ownerProvider_(),
            std::move(callback));
    });
    timer.set_function("every", [this](std::int64_t milliseconds, sol::protected_function callback) {
        return timers_.Every(
            std::chrono::milliseconds{std::max<std::int64_t>(milliseconds, 1)},
            ownerProvider_(),
            std::move(callback));
    });
    timer.set_function("cancel", [this](const std::uint64_t id) {
        return timers_.Cancel(id);
    });
}

void LuaBindingLibrary::RegisterUI() {
    sol::table ui = lua_.create_named_table("ui");
    ui.set_function("text", [this](const std::string& text) {
        return ui_.AddText(ownerProvider_(), text);
    });
    ui.set_function("button", [this](const std::string& label, sol::protected_function callback) {
        return ui_.AddButton(ownerProvider_(), label, std::move(callback));
    });
    ui.set_function(
        "checkbox",
        [this](
            const std::string& label,
            const bool initialValue,
            sol::protected_function callback) {
            return ui_.AddCheckbox(
                ownerProvider_(),
                label,
                initialValue,
                std::move(callback));
        });
    ui.set_function("remove", [this](const std::uint64_t id) {
        return ui_.Remove(id);
    });
}

void LuaBindingLibrary::RegisterImGui() {
    RegisterLuaImGuiBindings(logger_, lua_, frameScopeProvider_);
}

void LuaBindingLibrary::RegisterApplication() {
    sol::table app = lua_.create_named_table("app");
    app.set_function("script_name", [this]() {
        return ownerProvider_();
    });
    app.set_function("time_ms", []() {
        const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    });
    app.set_function("refresh_scripts", [this]() {
        refreshCallback_();
    });
}

bool LuaBindingLibrary::Ready() const noexcept {
    return ready_ && registry_ != nullptr;
}

} // namespace smf::scripting

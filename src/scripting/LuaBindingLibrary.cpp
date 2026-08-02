#include "scripting/LuaBindingLibrary.hpp"

#include "logging/Logger.hpp"
#include "scripting/LuaCommands.hpp"
#include "scripting/LuaEvents.hpp"
#include "scripting/LuaModule.hpp"
#include "scripting/LuaModuleManager.hpp"
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
    LuaModuleManager& modules,
    OwnerProvider ownerProvider,
    RefreshCallback refreshCallback,
    MetadataCallback metadataCallback)
    : logger_(logger),
      lua_(lua),
      commands_(commands),
      events_(events),
      timers_(timers),
      ui_(ui),
      modules_(modules),
      ownerProvider_(std::move(ownerProvider)),
      refreshCallback_(std::move(refreshCallback)),
      metadataCallback_(std::move(metadataCallback)) {}

void LuaBindingLibrary::BindFeatureRegistry(features::FeatureRegistry& registry) noexcept { registry_ = &registry; }

void LuaBindingLibrary::RegisterCoreBindings() {
    if (ready_) return;
    RegisterLogging();
    RegisterCommands();
    RegisterEvents();
    RegisterTimers();
    RegisterUI();
    RegisterApplication();
    RegisterDirectApiV2();
    lua_["LUA_API_VERSION"] = "2.0.0";
    ready_ = true;
    logger_.Info("Lua binding library initialized: Direct Menu API v2.0.0.");
}

void LuaBindingLibrary::RegisterLogging() {
    sol::table log = lua_.create_named_table("log");
    log.set_function("trace", [this](const std::string& message){ logger_.Trace("[Lua] " + message); });
    log.set_function("debug", [this](const std::string& message){ logger_.Debug("[Lua] " + message); });
    log.set_function("info", [this](const std::string& message){ logger_.Info("[Lua] " + message); });
    log.set_function("warn", [this](const std::string& message){ logger_.Warning("[Lua] " + message); });
    log.set_function("error", [this](const std::string& message){ logger_.Error("[Lua] " + message); });
}

void LuaBindingLibrary::RegisterCommands() {
    sol::table command = lua_.create_named_table("command");
    command.set_function("exists", [this](const std::string& name){ return commands_.Exists(name); });
    command.set_function("execute", [this](const std::string& name){ return commands_.Execute(name); });
    command.set_function("list", [this](){
        sol::table result = lua_.create_table();
        std::size_t index = 1;
        for (const auto& info : commands_.List()) {
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
    event.set_function("on", [this](const std::string& name, sol::protected_function callback){ return events_.Subscribe(name, ownerProvider_(), std::move(callback)); });
    event.set_function("off", [this](const std::uint64_t id){ return events_.Unsubscribe(id); });
    event.set_function("emit", [this](const std::string& name){ events_.Emit(name); });
}

void LuaBindingLibrary::RegisterTimers() {
    sol::table timer = lua_.create_named_table("timer");
    timer.set_function("after", [this](std::int64_t ms, sol::protected_function callback){
        return timers_.After(std::chrono::milliseconds{std::max<std::int64_t>(ms, 0)}, ownerProvider_(), std::move(callback));
    });
    timer.set_function("every", [this](std::int64_t ms, sol::protected_function callback){
        return timers_.Every(std::chrono::milliseconds{std::max<std::int64_t>(ms, 1)}, ownerProvider_(), std::move(callback));
    });
    timer.set_function("cancel", [this](const std::uint64_t id){ return timers_.Cancel(id); });
}

void LuaBindingLibrary::RegisterUI() {
    sol::table ui = lua_.create_named_table("ui");
    ui.set_function("text", [this](const std::string& text){ return ui_.AddText(ownerProvider_(), text); });
    ui.set_function("button", [this](const std::string& label, sol::protected_function callback){ return ui_.AddButton(ownerProvider_(), label, std::move(callback)); });
    ui.set_function("checkbox", [this](const std::string& label, const bool initialValue, sol::protected_function callback){
        return ui_.AddCheckbox(ownerProvider_(), label, initialValue, std::move(callback));
    });
    ui.set_function("remove", [this](const std::uint64_t id){ return ui_.Remove(id); });
}

void LuaBindingLibrary::RegisterApplication() {
    sol::table app = lua_.create_named_table("app");
    app.set_function("script_name", [this](){ return ownerProvider_(); });
    app.set_function("time_ms", [](){
        const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    });
    app.set_function("refresh_scripts", [this](){ refreshCallback_(); });
}

void LuaBindingLibrary::RegisterDirectApiV2() {
    sol::table direct = lua_.create_named_table("direct");

    sol::table version = lua_.create_table();
    version["major"] = 2;
    version["minor"] = 0;
    version["patch"] = 0;
    version["string"] = "2.0.0";
    direct["api_version"] = version;

    direct["log"] = lua_["log"];
    direct["command"] = lua_["command"];
    direct["events"] = lua_["event"];
    direct["timer"] = lua_["timer"];
    direct["ui"] = lua_["ui"];
    direct["app"] = lua_["app"];

    direct.set_function("plugin", [this](sol::table manifest) {
        const std::string author = manifest.get_or("author", std::string{});
        const std::string versionText = manifest.get_or("version", std::string{"0.0.0"});
        const std::string description = manifest.get_or("description", std::string{});
        const std::string apiVersion = manifest.get_or("api", std::string{"2.0"});
        std::vector<std::string> permissions;

        const sol::object permissionObject = manifest["permissions"];
        if (permissionObject.valid() && permissionObject.get_type() == sol::type::table) {
            const sol::table requested = permissionObject.as<sol::table>();
            for (const auto& item : requested) {
                const sol::object value = item.second;
                if (value.is<std::string>()) {
                    permissions.push_back(value.as<std::string>());
                }
            }
        }

        if (metadataCallback_) {
            metadataCallback_(ownerProvider_(), author, versionText, description, apiVersion, std::move(permissions));
        }
        return manifest;
    });

    sol::table module = lua_.create_table();
    module.set_function("exists", [this](const std::string& name){ return modules_.Find(name) != nullptr; });
    module.set_function("version", [this](const std::string& name) -> std::string {
        const LuaModule* found = modules_.Find(name);
        return found == nullptr ? std::string{} : std::string{found->Version()};
    });
    module.set_function("count", [this](){ return modules_.Size(); });
    direct["module"] = std::move(module);
}

bool LuaBindingLibrary::Ready() const noexcept { return ready_ && registry_ != nullptr; }

} // namespace smf::scripting

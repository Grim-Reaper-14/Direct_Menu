#include "scripting/LuaScriptsManager.hpp"

#include "logging/Logger.hpp"

#include <algorithm>
#include <format>
#include <system_error>
#include <utility>

namespace smf::scripting {

LuaScriptsManager::LuaScriptsManager(logging::LoggerApi& logger, sol::state& lua, OwnerChangedCallback ownerChanged, CleanupCallback cleanup)
    : logger_(logger), lua_(lua), ownerChanged_(std::move(ownerChanged)), cleanup_(std::move(cleanup)) {}

void LuaScriptsManager::Initialize(std::filesystem::path scriptsDirectory) {
    scriptsDirectory_ = std::move(scriptsDirectory);
    Refresh();
    nextHotReloadCheck_ = std::chrono::steady_clock::now();
}

void LuaScriptsManager::Refresh() {
    std::vector<ScriptRecord> discovered;
    std::error_code error;
    if (scriptsDirectory_.empty()) { scripts_ = {}; return; }
    std::filesystem::create_directories(scriptsDirectory_, error);
    if (error) { logger_.Warning("Lua scripts directory could not be created: " + error.message()); return; }

    for (const auto& entry : std::filesystem::directory_iterator(scriptsDirectory_, error)) {
        if (error) break;
        if (!entry.is_regular_file(error) || entry.path().extension() != L".lua") continue;
        ScriptRecord record{};
        record.name = entry.path().filename().string();
        record.path = entry.path();
        if (const ScriptRecord* existing = Find(record.name)) {
            record.loaded = existing->loaded;
            record.enabled = existing->enabled;
            record.autoLoad = existing->autoLoad;
            record.author = existing->author;
            record.version = existing->version;
            record.description = existing->description;
            record.apiVersion = existing->apiVersion;
            record.permissions = existing->permissions;
            record.lastError = existing->lastError;
        }
        discovered.push_back(std::move(record));
    }
    std::ranges::sort(discovered, [](const ScriptRecord& a, const ScriptRecord& b){ return a.name < b.name; });
    scripts_ = std::move(discovered);
}

void LuaScriptsManager::Update() {
    const auto now = std::chrono::steady_clock::now();
    if (now < nextHotReloadCheck_) return;
    nextHotReloadCheck_ = now + std::chrono::milliseconds{500};
    std::vector<std::string> changed;
    for (const ScriptRecord& script : scripts_) {
        if (!script.loaded) continue;
        std::error_code error;
        const auto writeTime = std::filesystem::last_write_time(script.path, error);
        if (error) continue;
        const auto it = writeTimes_.find(script.name);
        if (it == writeTimes_.end()) { writeTimes_[script.name] = writeTime; continue; }
        if (it->second != writeTime) { it->second = writeTime; changed.push_back(script.name); }
    }
    for (const auto& name : changed) {
        logger_.Info("Lua script changed on disk; hot reloading: " + name);
        Reload(name);
    }
}

bool LuaScriptsManager::Load(const std::string_view name) {
    ScriptRecord* script = Find(name);
    if (!script || !script->enabled) return false;
    if (script->loaded) return true;
    return ExecuteScript(*script);
}

bool LuaScriptsManager::ExecuteScript(ScriptRecord& script) {
    cleanup_(script.name);
    runtimeScripts_.erase(script.name);
    script.permissions.clear();
    sol::environment environment{lua_, sol::create, lua_.globals()};
    environment["SCRIPT_NAME"] = script.name;
    environment["SCRIPT_PATH"] = script.path.string();

    ownerChanged_(script.name);
    const sol::load_result loaded = lua_.load_file(script.path.string());
    if (!loaded.valid()) {
        const sol::error error = loaded;
        ownerChanged_({});
        script.loaded = false;
        script.lastError = error.what();
        logger_.Error(std::format("Lua load failed for '{}': {}", script.name, error.what()));
        return false;
    }

    sol::protected_function function = loaded;
    function.set_environment(environment);
    const sol::protected_function_result result = function();
    ownerChanged_({});
    if (!result.valid()) {
        const sol::error error = result;
        script.loaded = false;
        script.lastError = error.what();
        cleanup_(script.name);
        logger_.Error(std::format("Lua execution failed for '{}': {}", script.name, error.what()));
        return false;
    }

    runtimeScripts_.insert_or_assign(script.name, RuntimeScript{.environment = std::move(environment)});
    script.loaded = true;
    script.lastError.clear();
    RememberWriteTime(script);
    logger_.Info("Lua script loaded: " + script.name);
    return true;
}

void LuaScriptsManager::RememberWriteTime(const ScriptRecord& script) {
    std::error_code error;
    const auto writeTime = std::filesystem::last_write_time(script.path, error);
    if (!error) writeTimes_[script.name] = writeTime;
}

void LuaScriptsManager::InvokeUnload(const std::string_view name) {
    const auto it = runtimeScripts_.find(std::string{name});
    if (it == runtimeScripts_.end()) return;
    const sol::object unloadObject = it->second.environment["on_unload"];
    if (!unloadObject.valid() || unloadObject.get_type() != sol::type::function) return;
    ownerChanged_(name);
    sol::protected_function unload = unloadObject.as<sol::protected_function>();
    const auto result = unload();
    ownerChanged_({});
    if (!result.valid()) { const sol::error error = result; logger_.Error(std::format("Lua on_unload failed for '{}': {}", name, error.what())); }
}

bool LuaScriptsManager::Unload(const std::string_view name) {
    ScriptRecord* script = Find(name);
    if (!script || !script->loaded) return false;
    InvokeUnload(name);
    cleanup_(name);
    runtimeScripts_.erase(std::string{name});
    writeTimes_.erase(std::string{name});
    script->loaded = false;
    logger_.Info("Lua script unloaded: " + script->name);
    return true;
}

bool LuaScriptsManager::Reload(const std::string_view name) {
    ScriptRecord* script = Find(name);
    if (!script) return false;
    if (script->loaded) Unload(name);
    return Load(name);
}

void LuaScriptsManager::UnloadAll() {
    std::vector<std::string> loaded;
    for (const auto& script : scripts_) if (script.loaded) loaded.push_back(script.name);
    for (const auto& name : loaded) Unload(name);
    runtimeScripts_.clear();
    writeTimes_.clear();
}

ScriptRecord* LuaScriptsManager::Find(const std::string_view name) noexcept {
    const auto it = std::ranges::find_if(scripts_, [name](const ScriptRecord& script){ return script.name == name; });
    return it == scripts_.end() ? nullptr : &*it;
}

const ScriptRecord* LuaScriptsManager::Find(const std::string_view name) const noexcept {
    const auto it = std::ranges::find_if(scripts_, [name](const ScriptRecord& script){ return script.name == name; });
    return it == scripts_.end() ? nullptr : &*it;
}

bool LuaScriptsManager::HasPermission(
    const std::string_view owner,
    const std::string_view permission) const noexcept {
    const ScriptRecord* script = Find(owner);
    if (script == nullptr) {
        return false;
    }

    if (std::ranges::find(script->permissions, std::string{permission}) != script->permissions.end()) {
        return true;
    }

    const auto separator = permission.find('.');
    if (separator != std::string_view::npos) {
        const std::string parent{permission.substr(0, separator)};
        return std::ranges::find(script->permissions, parent) != script->permissions.end();
    }
    return false;
}

const std::vector<ScriptRecord>& LuaScriptsManager::Scripts() const noexcept { return scripts_; }
const std::filesystem::path& LuaScriptsManager::Directory() const noexcept { return scriptsDirectory_; }

} // namespace smf::scripting

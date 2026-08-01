#include "scripting/LuaScriptsManager.hpp"

#include "logging/Logger.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace smf::scripting {

LuaScriptsManager::LuaScriptsManager(logging::LoggerApi& logger)
    : logger_(logger) {
}

void LuaScriptsManager::Initialize(std::filesystem::path scriptsDirectory) {
    scriptsDirectory_ = std::move(scriptsDirectory);
    Refresh();
}

void LuaScriptsManager::Refresh() {
    std::vector<ScriptRecord> discovered;

    std::error_code error;
    if (scriptsDirectory_.empty()) {
        scripts_ = std::move(discovered);
        return;
    }

    std::filesystem::create_directories(scriptsDirectory_, error);
    if (error) {
        logger_.Warning("Lua scripts directory could not be created: " + error.message());
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(scriptsDirectory_, error)) {
        if (error) {
            logger_.Warning("Lua scripts directory scan stopped: " + error.message());
            break;
        }

        if (!entry.is_regular_file(error) || entry.path().extension() != L".lua") {
            continue;
        }

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
            record.lastError = existing->lastError;
        }

        discovered.push_back(std::move(record));
    }

    std::ranges::sort(
        discovered,
        [](const ScriptRecord& left, const ScriptRecord& right) {
            return left.name < right.name;
        });

    scripts_ = std::move(discovered);
}

bool LuaScriptsManager::Load(const std::string_view name) {
    ScriptRecord* script = Find(name);
    if (script == nullptr || !script->enabled) {
        return false;
    }

    // The runtime execution hook is intentionally isolated here. Once the
    // Lua runtime is linked, this is where compilation/environment creation
    // belongs instead of spreading runtime calls throughout the application.
    script->loaded = true;
    script->lastError.clear();
    logger_.Info("Lua script marked loaded: " + script->name);
    return true;
}

bool LuaScriptsManager::Unload(const std::string_view name) {
    ScriptRecord* script = Find(name);
    if (script == nullptr || !script->loaded) {
        return false;
    }

    script->loaded = false;
    logger_.Info("Lua script unloaded: " + script->name);
    return true;
}

bool LuaScriptsManager::Reload(const std::string_view name) {
    ScriptRecord* script = Find(name);
    if (script == nullptr) {
        return false;
    }

    if (script->loaded) {
        Unload(name);
    }
    return Load(name);
}

void LuaScriptsManager::UnloadAll() {
    for (ScriptRecord& script : scripts_) {
        if (script.loaded) {
            script.loaded = false;
            logger_.Debug("Lua script unloaded during shutdown: " + script.name);
        }
    }
}

ScriptRecord* LuaScriptsManager::Find(const std::string_view name) noexcept {
    const auto it = std::ranges::find_if(
        scripts_,
        [name](const ScriptRecord& script) {
            return script.name == name;
        });
    return it == scripts_.end() ? nullptr : &*it;
}

const ScriptRecord* LuaScriptsManager::Find(const std::string_view name) const noexcept {
    const auto it = std::ranges::find_if(
        scripts_,
        [name](const ScriptRecord& script) {
            return script.name == name;
        });
    return it == scripts_.end() ? nullptr : &*it;
}

const std::vector<ScriptRecord>& LuaScriptsManager::Scripts() const noexcept {
    return scripts_;
}

const std::filesystem::path& LuaScriptsManager::Directory() const noexcept {
    return scriptsDirectory_;
}

} // namespace smf::scripting

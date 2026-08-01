#include "scripting/LuaManager.hpp"

#include "logging/Logger.hpp"

#include <utility>

namespace smf::scripting {

LuaManager::LuaManager(logging::LoggerApi& logger)
    : logger_(logger),
      bindings_(logger_, commands_),
      scripts_(logger_),
      modules_(*this) {
}

LuaManager::~LuaManager() {
    Shutdown();
}

void LuaManager::Initialize(std::filesystem::path scriptsDirectory) {
    if (initialized_) {
        return;
    }

    scripts_.Initialize(std::move(scriptsDirectory));
    bindings_.RegisterCoreBindings();
    initialized_ = true;

    logger_.Info("Lua subsystem architecture initialized.");
}

void LuaManager::Shutdown() noexcept {
    if (!initialized_) {
        return;
    }

    modules_.Shutdown();
    scripts_.UnloadAll();
    initialized_ = false;
    logger_.Info("Lua subsystem architecture shut down.");
}

void LuaManager::Update() {
    if (!initialized_) {
        return;
    }

    modules_.Update();
}

void LuaManager::Refresh() {
    scripts_.Refresh();
}

const std::vector<ScriptRecord>& LuaManager::Scripts() const noexcept {
    return scripts_.Scripts();
}

bool LuaManager::RuntimeReady() const noexcept {
    return initialized_ && bindings_.Ready();
}

std::string LuaManager::StatusText() const {
    if (!initialized_) {
        return "Lua subsystem is not initialized.";
    }

    if (!bindings_.Ready()) {
        return "Lua subsystem initialized; waiting for the native feature registry binding.";
    }

    return "Lua managers, command registry, module system, and binding boundary are ready; Lua VM execution remains isolated behind LuaScriptsManager.";
}

void LuaManager::BindFeatureRegistry(features::FeatureRegistry& registry) {
    bindings_.BindFeatureRegistry(registry);
    logger_.Debug("Feature registry attached to the Lua binding library.");
}

LuaScriptsManager& LuaManager::ScriptsManager() noexcept {
    return scripts_;
}

LuaModuleManager& LuaManager::Modules() noexcept {
    return modules_;
}

LuaCommands& LuaManager::Commands() noexcept {
    return commands_;
}

LuaBindingLibrary& LuaManager::Bindings() noexcept {
    return bindings_;
}

} // namespace smf::scripting

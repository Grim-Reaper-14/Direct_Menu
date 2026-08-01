#include "scripting/LuaManager.hpp"

#include "logging/Logger.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <utility>

namespace smf::scripting {

LuaManager::LuaManager(logging::LoggerApi& logger)
    : logger_(logger),
      events_(logger_),
      timers_(logger_),
      ui_(logger_),
      bindings_(
          logger_,
          luaState_,
          commands_,
          events_,
          timers_,
          ui_,
          [this] { return ActiveScriptName(); },
          [this] { Refresh(); }),
      scripts_(
          logger_,
          luaState_,
          [this](const std::string_view owner) { SetActiveScript(owner); },
          [this](const std::string_view owner) { CleanupOwnedResources(owner); }),
      modules_(*this) {
}

LuaManager::~LuaManager() {
    Shutdown();
}

void LuaManager::Initialize(std::filesystem::path scriptsDirectory) {
    if (initialized_) {
        return;
    }

    OpenLibraries();
    scripts_.Initialize(std::move(scriptsDirectory));
    bindings_.RegisterCoreBindings();
    initialized_ = true;

    for (const ScriptRecord& script : scripts_.Scripts()) {
        if (script.enabled && script.autoLoad) {
            (void)scripts_.Load(script.name);
        }
    }

    InstallFrameHook();
    logger_.Info("Lua 5.4 runtime and sol2 binding layer initialized.");
}

void LuaManager::OpenLibraries() {
    luaState_.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::coroutine,
        sol::lib::utf8);

    luaState_["io"] = sol::nil;
    luaState_["os"] = sol::nil;
    luaState_["debug"] = sol::nil;
    luaState_["package"] = sol::nil;
    luaState_["require"] = sol::nil;
    luaState_["dofile"] = sol::nil;
    luaState_["loadfile"] = sol::nil;
}

void LuaManager::InstallFrameHook() {
    if (imguiHookId_ != 0 || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImGuiContextHook hook{};
    hook.Type = ImGuiContextHookType_NewFramePost;
    hook.UserData = this;
    hook.Callback = [](ImGuiContext*, ImGuiContextHook* contextHook) {
        auto* manager = static_cast<LuaManager*>(contextHook->UserData);
        if (manager == nullptr) {
            return;
        }
        manager->Update();
        manager->Draw();
    };

    imguiHookId_ = ImGui::AddContextHook(ImGui::GetCurrentContext(), &hook);
    logger_.Debug("Lua per-frame ImGui hook installed.");
}

void LuaManager::RemoveFrameHook() noexcept {
    if (imguiHookId_ == 0 || ImGui::GetCurrentContext() == nullptr) {
        imguiHookId_ = 0;
        return;
    }

    ImGui::RemoveContextHook(ImGui::GetCurrentContext(), imguiHookId_);
    imguiHookId_ = 0;
}

void LuaManager::Shutdown() noexcept {
    if (!initialized_) {
        return;
    }

    RemoveFrameHook();
    events_.Emit("shutdown");
    scripts_.UnloadAll();
    modules_.Shutdown();
    timers_.Clear();
    events_.Clear();
    ui_.Clear();
    activeScriptName_.clear();
    initialized_ = false;
    logger_.Info("Lua subsystem shut down.");
}

void LuaManager::Update() {
    if (!initialized_) {
        return;
    }

    scripts_.Update();
    timers_.Update();
    events_.Emit("tick");
    modules_.Update();
}

void LuaManager::Draw() {
    if (!initialized_) {
        return;
    }

    events_.Emit("draw");
    ui_.Draw();
}

void LuaManager::Refresh() {
    scripts_.Refresh();
    for (const ScriptRecord& script : scripts_.Scripts()) {
        if (script.enabled && script.autoLoad && !script.loaded) {
            (void)scripts_.Load(script.name);
        }
    }
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
        return "Lua 5.4 runtime is initialized; waiting for the native feature registry binding.";
    }

    return "Lua 5.4 + sol2 runtime ready. Scripts autoload, hot reload on file changes, and support events, timers, commands, and retained ImGui widgets.";
}

std::string LuaManager::ActiveScriptName() const {
    return activeScriptName_.empty() ? "__native__" : activeScriptName_;
}

void LuaManager::SetActiveScript(const std::string_view owner) {
    activeScriptName_.assign(owner.begin(), owner.end());
}

void LuaManager::CleanupOwnedResources(const std::string_view owner) {
    commands_.UnregisterByOwner(owner);
    events_.RemoveByOwner(owner);
    timers_.RemoveByOwner(owner);
    ui_.RemoveByOwner(owner);
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

LuaEvents& LuaManager::Events() noexcept {
    return events_;
}

LuaTimerManager& LuaManager::Timers() noexcept {
    return timers_;
}

LuaUI& LuaManager::UI() noexcept {
    return ui_;
}

sol::state& LuaManager::State() noexcept {
    return luaState_;
}

} // namespace smf::scripting

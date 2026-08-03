#include "scripting/LuaManager.hpp"

#include "logging/Logger.hpp"

#include <imgui.h>

#include <utility>

namespace smf::scripting {

LuaManager::LuaManager(logging::LoggerApi& logger)
    : logger_(logger),
      events_(logger_, [this](const std::string_view owner) { SetActiveScript(owner); }),
      timers_(logger_, [this](const std::string_view owner) { SetActiveScript(owner); }),
      ui_(logger_, [this](const std::string_view owner) { SetActiveScript(owner); }),
      modules_(*this),
      fileSystemSandbox_(logger_),
      bindings_(
          logger_,
          luaState_,
          commands_,
          events_,
          timers_,
          ui_,
          modules_,
          fileSystemSandbox_,
          [this] { return ActiveScriptName(); },
          [this] { return drawingFrame_; },
          [this] { Refresh(); },
          [this](const std::string_view owner, const std::string_view permission) {
              return scripts_.HasPermission(owner, permission);
          },
          [this](
              const std::string_view owner,
              std::string author,
              std::string version,
              std::string description,
              std::string apiVersion,
              std::vector<std::string> permissions) {
              if (ScriptRecord* script = scripts_.Find(owner)) {
                  script->author = std::move(author);
                  script->version = std::move(version);
                  script->description = std::move(description);
                  script->apiVersion = std::move(apiVersion);
                  script->permissions = std::move(permissions);
              }
          }),
      scripts_(
          logger_,
          luaState_,
          [this](const std::string_view owner) { SetActiveScript(owner); },
          [this](const std::string_view owner) { CleanupOwnedResources(owner); }) {
}

LuaManager::~LuaManager() { Shutdown(); }

void LuaManager::Initialize(std::filesystem::path scriptsDirectory) {
    if (initialized_) return;
    if (ImGui::GetCurrentContext() == nullptr) {
        logger_.Error("Lua initialization requires an active ImGui context.");
        return;
    }

    const std::filesystem::path sandboxRoot = scriptsDirectory / ".sandbox";
    if (!fileSystemSandbox_.Initialize(sandboxRoot)) {
        logger_.Warning("Lua filesystem sandbox could not be initialized; filesystem API calls will fail closed.");
    }

    OpenLibraries();
    scripts_.Initialize(std::move(scriptsDirectory));
    bindings_.RegisterCoreBindings();
    initialized_ = true;
    for (const ScriptRecord& script : scripts_.Scripts()) {
        if (script.enabled && script.autoLoad) (void)scripts_.Load(script.name);
    }
    logger_.Info(
        "Lua 5.4 runtime and sol2 API v2.1 binding layer initialized. Script folder: " +
        scripts_.Directory().string());
}

void LuaManager::OpenLibraries() {
    luaState_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::coroutine, sol::lib::utf8);
    luaState_["io"] = sol::nil;
    luaState_["os"] = sol::nil;
    luaState_["debug"] = sol::nil;
    luaState_["package"] = sol::nil;
    luaState_["require"] = sol::nil;
    luaState_["dofile"] = sol::nil;
    luaState_["loadfile"] = sol::nil;
}

void LuaManager::Shutdown() noexcept {
    if (!initialized_) return;
    events_.Emit("shutdown");
    scripts_.UnloadAll();
    modules_.Shutdown();
    timers_.Clear();
    events_.Clear();
    ui_.Clear();
    activeScriptName_.clear();
    memoryApiBound_ = false;
    sdkApiBound_ = false;
    initialized_ = false;
    logger_.Info("Lua subsystem shut down.");
}

void LuaManager::Update() {
    if (!initialized_) return;
    scripts_.Update();
    timers_.Update();
    events_.Emit("tick");
    modules_.Update();
}

void LuaManager::Draw() {
    if (!initialized_) return;
    drawingFrame_ = true;
    events_.Emit("draw");
    drawingFrame_ = false;
}

void LuaManager::DrawMenu() {
    if (!initialized_) return;
    drawingFrame_ = true;
    events_.Emit("menu");
    ui_.DrawInline();
    drawingFrame_ = false;
}

void LuaManager::Refresh() {
    scripts_.Refresh();
    for (const ScriptRecord& script : scripts_.Scripts()) {
        if (script.enabled && script.autoLoad && !script.loaded) {
            (void)scripts_.Load(script.name);
        }
    }
}

const std::vector<ScriptRecord>& LuaManager::Scripts() const noexcept { return scripts_.Scripts(); }
bool LuaManager::RuntimeReady() const noexcept { return initialized_ && bindings_.Ready(); }
bool LuaManager::HasMenuContent() const noexcept {
    return initialized_ &&
           (events_.HasSubscribers("menu") || !ui_.Empty());
}

std::string LuaManager::StatusText() const {
    if (!initialized_) return "Lua subsystem is not initialized.";
    if (!bindings_.Ready()) return "Lua 5.4 runtime is initialized; waiting for the native feature registry binding.";
    if (!memoryApiBound_) return "Lua API v2.1 ready; memory and signature bindings are not attached yet.";
    if (!sdkApiBound_) return "Lua API v2.1 ready; safe SDK handle bindings are not attached yet.";
    return "Lua API v2.1 ready: Lua 5.4 + sol2 with UI, modules, commands, events, timers, sandboxed files, read-only Memory/Signatures, and safe SDK handle APIs.";
}

std::string LuaManager::ActiveScriptName() const { return activeScriptName_.empty() ? "__native__" : activeScriptName_; }
void LuaManager::SetActiveScript(const std::string_view owner) { activeScriptName_.assign(owner.begin(), owner.end()); }

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

void LuaManager::BindMemoryAPI(
    core::MemoryManagerAPI& memory,
    core::SignatureManager& signatures) {
    if (memoryApiBound_) {
        return;
    }

    LuaMemoryBindings::Register(luaState_, memory, signatures);
    memoryApiBound_ = true;
    logger_.Debug("Read-only Memory and Signatures APIs attached to Lua.");
}

void LuaManager::BindSDK(sdk::SDK& services) {
    if (sdkApiBound_) {
        return;
    }

    LuaSDKBindings::Register(luaState_, services);
    sdkApiBound_ = true;
    logger_.Debug("Safe SDK handle APIs attached to Lua.");
}

LuaScriptsManager& LuaManager::ScriptsManager() noexcept { return scripts_; }
LuaModuleManager& LuaManager::Modules() noexcept { return modules_; }
LuaCommands& LuaManager::Commands() noexcept { return commands_; }
LuaBindingLibrary& LuaManager::Bindings() noexcept { return bindings_; }
LuaEvents& LuaManager::Events() noexcept { return events_; }
LuaTimerManager& LuaManager::Timers() noexcept { return timers_; }
LuaUI& LuaManager::UI() noexcept { return ui_; }
LuaFileSystemSandbox& LuaManager::FileSystemSandbox() noexcept { return fileSystemSandbox_; }
sol::state& LuaManager::State() noexcept { return luaState_; }

} // namespace smf::scripting

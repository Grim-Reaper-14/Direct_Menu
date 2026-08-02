#pragma once

#include "scripting/LuaBindingLibrary.hpp"
#include "scripting/LuaCommands.hpp"
#include "scripting/LuaEvents.hpp"
#include "scripting/LuaFileSystemSandbox.hpp"
#include "scripting/LuaModuleManager.hpp"
#include "scripting/LuaScriptsManager.hpp"
#include "scripting/LuaTimerManager.hpp"
#include "scripting/LuaUI.hpp"

#include <sol/sol.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace smf::features { class FeatureRegistry; }
namespace smf::logging { class LoggerApi; }
namespace smf::scripting {

class LuaManager {
public:
    explicit LuaManager(logging::LoggerApi& logger);
    ~LuaManager();
    LuaManager(const LuaManager&) = delete;
    LuaManager& operator=(const LuaManager&) = delete;

    void Initialize(std::filesystem::path scriptsDirectory);
    void Shutdown() noexcept;
    void Update();
    void Draw();
    void DrawMenu();
    void Refresh();

    [[nodiscard]] const std::vector<ScriptRecord>& Scripts() const noexcept;
    [[nodiscard]] bool RuntimeReady() const noexcept;
    [[nodiscard]] bool HasMenuContent() const noexcept;
    [[nodiscard]] std::string StatusText() const;
    [[nodiscard]] std::string ActiveScriptName() const;

    void BindFeatureRegistry(features::FeatureRegistry& registry);

    [[nodiscard]] LuaScriptsManager& ScriptsManager() noexcept;
    [[nodiscard]] LuaModuleManager& Modules() noexcept;
    [[nodiscard]] LuaCommands& Commands() noexcept;
    [[nodiscard]] LuaBindingLibrary& Bindings() noexcept;
    [[nodiscard]] LuaEvents& Events() noexcept;
    [[nodiscard]] LuaTimerManager& Timers() noexcept;
    [[nodiscard]] LuaUI& UI() noexcept;
    [[nodiscard]] LuaFileSystemSandbox& FileSystemSandbox() noexcept;
    [[nodiscard]] sol::state& State() noexcept;

private:
    void OpenLibraries();
    void SetActiveScript(std::string_view owner);
    void CleanupOwnedResources(std::string_view owner);

    logging::LoggerApi& logger_;
    sol::state luaState_;
    LuaCommands commands_;
    LuaEvents events_;
    LuaTimerManager timers_;
    LuaUI ui_;
    LuaModuleManager modules_;
    LuaFileSystemSandbox fileSystemSandbox_;
    LuaBindingLibrary bindings_;
    LuaScriptsManager scripts_;
    std::string activeScriptName_;
    bool drawingFrame_{false};
    bool initialized_{false};
};

} // namespace smf::scripting

#pragma once

#include "scripting/LuaBindingLibrary.hpp"
#include "scripting/LuaCommands.hpp"
#include "scripting/LuaModuleManager.hpp"
#include "scripting/LuaScriptsManager.hpp"

#include <filesystem>
#include <string>

namespace smf::features {
class FeatureRegistry;
}

namespace smf::logging {
class LoggerApi;
}

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
    void Refresh();

    [[nodiscard]] const std::vector<ScriptRecord>& Scripts() const noexcept;
    [[nodiscard]] bool RuntimeReady() const noexcept;
    [[nodiscard]] std::string StatusText() const;

    void BindFeatureRegistry(features::FeatureRegistry& registry);

    [[nodiscard]] LuaScriptsManager& ScriptsManager() noexcept;
    [[nodiscard]] LuaModuleManager& Modules() noexcept;
    [[nodiscard]] LuaCommands& Commands() noexcept;
    [[nodiscard]] LuaBindingLibrary& Bindings() noexcept;

private:
    logging::LoggerApi& logger_;
    LuaCommands commands_;
    LuaBindingLibrary bindings_;
    LuaScriptsManager scripts_;
    LuaModuleManager modules_;
    bool initialized_{false};
};

} // namespace smf::scripting

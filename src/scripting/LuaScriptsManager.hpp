#pragma once

#include <sol/sol.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smf::logging { class LoggerApi; }
namespace smf::scripting {

struct ScriptRecord {
    std::string name;
    std::filesystem::path path;
    bool loaded{false};
    bool enabled{true};
    bool autoLoad{true};
    std::string author;
    std::string version;
    std::string description;
    std::string apiVersion{"2.0"};
    std::vector<std::string> permissions;
    std::string lastError;
};

class LuaScriptsManager {
public:
    using OwnerChangedCallback = std::function<void(std::string_view)>;
    using CleanupCallback = std::function<void(std::string_view)>;

    LuaScriptsManager(logging::LoggerApi& logger, sol::state& lua, OwnerChangedCallback ownerChanged, CleanupCallback cleanup);
    void Initialize(std::filesystem::path scriptsDirectory);
    void Refresh();
    void Update();
    [[nodiscard]] bool Load(std::string_view name);
    [[nodiscard]] bool Unload(std::string_view name);
    [[nodiscard]] bool Reload(std::string_view name);
    void UnloadAll();
    [[nodiscard]] ScriptRecord* Find(std::string_view name) noexcept;
    [[nodiscard]] const ScriptRecord* Find(std::string_view name) const noexcept;
    [[nodiscard]] const std::vector<ScriptRecord>& Scripts() const noexcept;
    [[nodiscard]] const std::filesystem::path& Directory() const noexcept;

private:
    struct RuntimeScript { sol::environment environment; };
    [[nodiscard]] bool ExecuteScript(ScriptRecord& script);
    void InvokeUnload(std::string_view name);
    void RememberWriteTime(const ScriptRecord& script);

    logging::LoggerApi& logger_;
    sol::state& lua_;
    OwnerChangedCallback ownerChanged_;
    CleanupCallback cleanup_;
    std::filesystem::path scriptsDirectory_;
    std::vector<ScriptRecord> scripts_;
    std::unordered_map<std::string, RuntimeScript> runtimeScripts_;
    std::unordered_map<std::string, std::filesystem::file_time_type> writeTimes_;
    std::chrono::steady_clock::time_point nextHotReloadCheck_{};
};

} // namespace smf::scripting

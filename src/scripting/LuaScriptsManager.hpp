#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

struct ScriptRecord {
    std::string name;
    std::filesystem::path path;
    bool loaded{false};
    bool enabled{true};
    bool autoLoad{false};
    std::string author;
    std::string version;
    std::string description;
    std::string lastError;
};

class LuaScriptsManager {
public:
    explicit LuaScriptsManager(logging::LoggerApi& logger);

    void Initialize(std::filesystem::path scriptsDirectory);
    void Refresh();

    [[nodiscard]] bool Load(std::string_view name);
    [[nodiscard]] bool Unload(std::string_view name);
    [[nodiscard]] bool Reload(std::string_view name);
    void UnloadAll();

    [[nodiscard]] ScriptRecord* Find(std::string_view name) noexcept;
    [[nodiscard]] const ScriptRecord* Find(std::string_view name) const noexcept;

    [[nodiscard]] const std::vector<ScriptRecord>& Scripts() const noexcept;
    [[nodiscard]] const std::filesystem::path& Directory() const noexcept;

private:
    logging::LoggerApi& logger_;
    std::filesystem::path scriptsDirectory_;
    std::vector<ScriptRecord> scripts_;
};

} // namespace smf::scripting

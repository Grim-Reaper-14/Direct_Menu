#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace smf::features {
class FeatureRegistry;
}

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

struct ScriptRecord {
    std::string name;
    std::filesystem::path path;
};

class LuaManager {
public:
    explicit LuaManager(logging::LoggerApi& logger);

    void Initialize(std::filesystem::path scriptsDirectory);
    void Refresh();

    [[nodiscard]] const std::vector<ScriptRecord>& Scripts() const noexcept;
    [[nodiscard]] bool RuntimeReady() const noexcept;
    [[nodiscard]] std::string StatusText() const;

    // Future integration point. Native features should be registered and tested
    // before a Lua runtime calls through this boundary.
    void BindFeatureRegistry(features::FeatureRegistry& registry);

private:
    logging::LoggerApi& logger_;
    std::filesystem::path scriptsDirectory_;
    std::vector<ScriptRecord> scripts_;
    features::FeatureRegistry* registry_{nullptr};
};

} // namespace smf::scripting


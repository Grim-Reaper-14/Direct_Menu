#include "scripting/LuaManager.hpp"

#include "logging/Logger.hpp"

#include <algorithm>
#include <system_error>

namespace smf::scripting {

LuaManager::LuaManager(logging::LoggerApi& logger)
    : logger_(logger) {
}

void LuaManager::Initialize(std::filesystem::path scriptsDirectory) {
    scriptsDirectory_ = std::move(scriptsDirectory);
    Refresh();
    logger_.Info("Lua integration boundary initialized; runtime is intentionally deferred.");
}

void LuaManager::Refresh() {
    scripts_.clear();

    std::error_code error;
    if (scriptsDirectory_.empty() ||
        !std::filesystem::exists(scriptsDirectory_, error)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(scriptsDirectory_, error)) {
        if (error) {
            break;
        }

        if (entry.is_regular_file(error) && entry.path().extension() == L".lua") {
            scripts_.push_back({
                entry.path().filename().string(),
                entry.path()
            });
        }
    }

    std::ranges::sort(
        scripts_,
        [](const ScriptRecord& left, const ScriptRecord& right) {
            return left.name < right.name;
        });
}

const std::vector<ScriptRecord>& LuaManager::Scripts() const noexcept {
    return scripts_;
}

bool LuaManager::RuntimeReady() const noexcept {
    return false;
}

std::string LuaManager::StatusText() const {
    return "Lua runtime deferred until the native feature API is complete.";
}

void LuaManager::BindFeatureRegistry(features::FeatureRegistry& registry) {
    registry_ = &registry;
    logger_.Debug("Feature registry attached to the future Lua integration boundary.");
}

} // namespace smf::scripting


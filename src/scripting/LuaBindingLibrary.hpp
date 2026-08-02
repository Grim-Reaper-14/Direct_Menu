#pragma once

#include <sol/sol.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace smf::features { class FeatureRegistry; }
namespace smf::logging { class LoggerApi; }
namespace smf::scripting {

class LuaCommands;
class LuaEvents;
class LuaModuleManager;
class LuaTimerManager;
class LuaUI;

class LuaBindingLibrary {
public:
    using OwnerProvider = std::function<std::string()>;
    using RefreshCallback = std::function<void()>;
    using MetadataCallback = std::function<void(
        std::string_view owner,
        std::string author,
        std::string version,
        std::string description,
        std::string apiVersion,
        std::vector<std::string> permissions)>;

    LuaBindingLibrary(
        logging::LoggerApi& logger,
        sol::state& lua,
        LuaCommands& commands,
        LuaEvents& events,
        LuaTimerManager& timers,
        LuaUI& ui,
        LuaModuleManager& modules,
        OwnerProvider ownerProvider,
        RefreshCallback refreshCallback,
        MetadataCallback metadataCallback);

    void BindFeatureRegistry(features::FeatureRegistry& registry) noexcept;
    void RegisterCoreBindings();
    [[nodiscard]] bool Ready() const noexcept;

private:
    void RegisterLogging();
    void RegisterCommands();
    void RegisterEvents();
    void RegisterTimers();
    void RegisterUI();
    void RegisterApplication();
    void RegisterDirectApiV2();

    logging::LoggerApi& logger_;
    sol::state& lua_;
    LuaCommands& commands_;
    LuaEvents& events_;
    LuaTimerManager& timers_;
    LuaUI& ui_;
    LuaModuleManager& modules_;
    OwnerProvider ownerProvider_;
    RefreshCallback refreshCallback_;
    MetadataCallback metadataCallback_;
    features::FeatureRegistry* registry_{nullptr};
    bool ready_{false};
};

} // namespace smf::scripting

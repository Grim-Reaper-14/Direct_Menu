#pragma once

#include <sol/sol.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace smf::features {
class FeatureRegistry;
}

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

class LuaCommands;
class LuaEvents;
class LuaTimerManager;
class LuaUI;

class LuaBindingLibrary {
public:
    using OwnerProvider = std::function<std::string()>;
    using RefreshCallback = std::function<void()>;

    LuaBindingLibrary(
        logging::LoggerApi& logger,
        sol::state& lua,
        LuaCommands& commands,
        LuaEvents& events,
        LuaTimerManager& timers,
        LuaUI& ui,
        OwnerProvider ownerProvider,
        RefreshCallback refreshCallback);

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

    logging::LoggerApi& logger_;
    sol::state& lua_;
    LuaCommands& commands_;
    LuaEvents& events_;
    LuaTimerManager& timers_;
    LuaUI& ui_;
    OwnerProvider ownerProvider_;
    RefreshCallback refreshCallback_;
    features::FeatureRegistry* registry_{nullptr};
    bool ready_{false};
};

} // namespace smf::scripting

#pragma once

namespace smf::features {
class FeatureRegistry;
}

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

class LuaCommands;

class LuaBindingLibrary {
public:
    LuaBindingLibrary(logging::LoggerApi& logger, LuaCommands& commands);

    void BindFeatureRegistry(features::FeatureRegistry& registry) noexcept;
    void RegisterCoreBindings();

    [[nodiscard]] bool Ready() const noexcept;

private:
    logging::LoggerApi& logger_;
    LuaCommands& commands_;
    features::FeatureRegistry* registry_{nullptr};
    bool ready_{false};
};

} // namespace smf::scripting

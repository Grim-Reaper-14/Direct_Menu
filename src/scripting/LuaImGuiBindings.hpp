#pragma once

#include <sol/sol.hpp>

#include <functional>

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

using ImGuiFrameScopeProvider = std::function<bool()>;

void RegisterLuaImGuiBindings(
    logging::LoggerApi& logger,
    sol::state& lua,
    ImGuiFrameScopeProvider frameScopeProvider);

} // namespace smf::scripting

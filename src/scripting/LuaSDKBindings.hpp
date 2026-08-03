#pragma once

#include <sol/sol.hpp>

namespace smf::sdk {
class SDK;
}

namespace smf::scripting {

class LuaSDKBindings final {
public:
    static void Register(sol::state& lua, sdk::SDK& services);
};

} // namespace smf::scripting

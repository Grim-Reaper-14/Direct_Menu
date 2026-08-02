#pragma once

#include <sol/sol.hpp>

namespace smf::core {
class MemoryManagerAPI;
class SignatureManager;
}

namespace smf::scripting {

class LuaMemoryBindings final {
public:
    static void Register(
        sol::state& lua,
        core::MemoryManagerAPI& memory,
        core::SignatureManager& signatures);
};

} // namespace smf::scripting

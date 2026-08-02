#pragma once

#include <string_view>

namespace smf::scripting {

class LuaManager;

class LuaModule {
public:
    virtual ~LuaModule() = default;
    [[nodiscard]] virtual std::string_view Name() const noexcept = 0;
    [[nodiscard]] virtual std::string_view Version() const noexcept = 0;
    virtual bool Initialize(LuaManager& manager) = 0;
    virtual void Update() {}
    virtual void Shutdown() noexcept = 0;
};

} // namespace smf::scripting

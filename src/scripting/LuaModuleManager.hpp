#pragma once

#include "scripting/LuaModule.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace smf::scripting {

class LuaManager;

class LuaModuleManager {
public:
    explicit LuaModuleManager(LuaManager& manager);
    ~LuaModuleManager();

    LuaModuleManager(const LuaModuleManager&) = delete;
    LuaModuleManager& operator=(const LuaModuleManager&) = delete;

    bool Register(std::unique_ptr<LuaModule> module);
    bool Remove(std::string_view name);
    [[nodiscard]] LuaModule* Find(std::string_view name) noexcept;
    [[nodiscard]] const LuaModule* Find(std::string_view name) const noexcept;

    void Update();
    void Shutdown() noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;

private:
    LuaManager& manager_;
    std::vector<std::unique_ptr<LuaModule>> modules_;
};

} // namespace smf::scripting

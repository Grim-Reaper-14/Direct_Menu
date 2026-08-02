#include "scripting/LuaModuleManager.hpp"

#include "scripting/LuaManager.hpp"

#include <algorithm>
#include <utility>

namespace smf::scripting {

LuaModuleManager::LuaModuleManager(LuaManager& manager) : manager_(manager) {}
LuaModuleManager::~LuaModuleManager() { Shutdown(); }

bool LuaModuleManager::Register(std::unique_ptr<LuaModule> module) {
    if (!module || Find(module->Name()) != nullptr) return false;
    if (!module->Initialize(manager_)) return false;
    modules_.push_back(std::move(module));
    return true;
}

bool LuaModuleManager::Remove(const std::string_view name) {
    const auto it = std::ranges::find_if(modules_, [name](const auto& module){ return module && module->Name() == name; });
    if (it == modules_.end()) return false;
    (*it)->Shutdown();
    modules_.erase(it);
    return true;
}

LuaModule* LuaModuleManager::Find(const std::string_view name) noexcept {
    const auto it = std::ranges::find_if(modules_, [name](const auto& module){ return module && module->Name() == name; });
    return it == modules_.end() ? nullptr : it->get();
}

const LuaModule* LuaModuleManager::Find(const std::string_view name) const noexcept {
    const auto it = std::ranges::find_if(modules_, [name](const auto& module){ return module && module->Name() == name; });
    return it == modules_.end() ? nullptr : it->get();
}

void LuaModuleManager::Update() { for (const auto& module : modules_) if (module) module->Update(); }
void LuaModuleManager::Shutdown() noexcept { for (auto it=modules_.rbegin(); it!=modules_.rend(); ++it) if (*it) (*it)->Shutdown(); modules_.clear(); }
std::size_t LuaModuleManager::Size() const noexcept { return modules_.size(); }

} // namespace smf::scripting

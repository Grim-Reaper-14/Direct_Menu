#include "scripting/LuaCommands.hpp"

#include <algorithm>

namespace smf::scripting {

bool LuaCommands::Register(
    std::string name,
    std::string description,
    Callback callback,
    std::string owner) {
    if (name.empty() || !callback || commands_.contains(name)) {
        return false;
    }

    commands_.emplace(
        std::move(name),
        Entry{
            .description = std::move(description),
            .owner = std::move(owner),
            .callback = std::move(callback)});
    return true;
}

bool LuaCommands::Unregister(const std::string_view name) {
    return commands_.erase(std::string{name}) > 0;
}

void LuaCommands::UnregisterByOwner(const std::string_view owner) {
    std::erase_if(
        commands_,
        [owner](const auto& item) {
            return item.second.owner == owner;
        });
}

bool LuaCommands::Execute(const std::string_view name) const {
    const auto it = commands_.find(std::string{name});
    if (it == commands_.end() || !it->second.callback) {
        return false;
    }

    it->second.callback();
    return true;
}

bool LuaCommands::Exists(const std::string_view name) const {
    return commands_.contains(std::string{name});
}

std::vector<LuaCommands::CommandInfo> LuaCommands::List() const {
    std::vector<CommandInfo> result;
    result.reserve(commands_.size());

    for (const auto& [name, entry] : commands_) {
        result.push_back({name, entry.description, entry.owner});
    }

    std::ranges::sort(
        result,
        [](const CommandInfo& left, const CommandInfo& right) {
            return left.name < right.name;
        });
    return result;
}

} // namespace smf::scripting

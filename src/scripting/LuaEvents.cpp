#include "scripting/LuaEvents.hpp"

#include "logging/Logger.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace smf::scripting {

LuaEvents::LuaEvents(logging::LoggerApi& logger, OwnerChangedCallback ownerChanged)
    : logger_(logger), ownerChanged_(std::move(ownerChanged)) {
}

std::uint64_t LuaEvents::Subscribe(
    std::string eventName,
    std::string owner,
    sol::protected_function callback) {
    if (eventName.empty() || !callback.valid()) return 0;
    const std::uint64_t id = nextId_++;
    handlers_[std::move(eventName)].push_back(Handler{
        .id = id,
        .owner = std::move(owner),
        .callback = std::move(callback)});
    return id;
}

bool LuaEvents::Unsubscribe(const std::uint64_t id) {
    for (auto& [name, handlers] : handlers_) {
        const auto oldSize = handlers.size();
        std::erase_if(handlers, [id](const Handler& handler) { return handler.id == id; });
        if (handlers.size() != oldSize) return true;
    }
    return false;
}

void LuaEvents::RemoveByOwner(const std::string_view owner) {
    for (auto& [name, handlers] : handlers_) {
        std::erase_if(handlers, [owner](const Handler& handler) { return handler.owner == owner; });
    }
}

void LuaEvents::Clear() { handlers_.clear(); }
void LuaEvents::Emit(const std::string_view eventName) { Emit<>(eventName); }

void LuaEvents::ReportError(
    const std::string_view owner,
    const std::string_view eventName,
    const std::string_view message) const {
    logger_.Error(std::format("Lua event '{}' failed for '{}': {}", eventName, owner, message));
}

} // namespace smf::scripting

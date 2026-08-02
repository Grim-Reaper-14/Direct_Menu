#include "scripting/LuaTimerManager.hpp"

#include "logging/Logger.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace smf::scripting {

LuaTimerManager::LuaTimerManager(logging::LoggerApi& logger)
    : logger_(logger) {
}

std::uint64_t LuaTimerManager::After(
    const std::chrono::milliseconds delay,
    std::string owner,
    sol::protected_function callback) {
    return Add(delay, false, std::move(owner), std::move(callback));
}

std::uint64_t LuaTimerManager::Every(
    const std::chrono::milliseconds interval,
    std::string owner,
    sol::protected_function callback) {
    return Add(
        std::max(interval, std::chrono::milliseconds{1}),
        true,
        std::move(owner),
        std::move(callback));
}

std::uint64_t LuaTimerManager::Add(
    const std::chrono::milliseconds delay,
    const bool repeating,
    std::string owner,
    sol::protected_function callback) {
    if (!callback.valid()) {
        return 0;
    }

    const std::uint64_t id = nextId_++;
    const auto safeDelay = std::max(delay, std::chrono::milliseconds{0});
    timers_.push_back(Timer{
        .id = id,
        .owner = std::move(owner),
        .next = std::chrono::steady_clock::now() + safeDelay,
        .interval = safeDelay,
        .repeating = repeating,
        .cancelled = false,
        .callback = std::move(callback)
    });
    return id;
}

bool LuaTimerManager::Cancel(const std::uint64_t id) {
    const auto iterator = std::ranges::find_if(
        timers_,
        [id](const Timer& timer) { return timer.id == id; });
    if (iterator == timers_.end()) {
        return false;
    }
    iterator->cancelled = true;
    return true;
}

void LuaTimerManager::RemoveByOwner(const std::string_view owner) {
    std::erase_if(timers_, [owner](const Timer& timer) {
        return timer.owner == owner;
    });
}

void LuaTimerManager::Clear() {
    timers_.clear();
}

void LuaTimerManager::Update() {
    const auto now = std::chrono::steady_clock::now();

    for (Timer& timer : timers_) {
        if (timer.cancelled || now < timer.next) {
            continue;
        }

        const sol::protected_function_result result = timer.callback(timer.id);
        if (!result.valid()) {
            const sol::error error = result;
            logger_.Error(std::format(
                "Lua timer {} owned by '{}' failed: {}",
                timer.id,
                timer.owner,
                error.what()));
            timer.cancelled = true;
            continue;
        }

        if (timer.repeating) {
            timer.next = now + timer.interval;
        } else {
            timer.cancelled = true;
        }
    }

    std::erase_if(timers_, [](const Timer& timer) {
        return timer.cancelled;
    });
}

} // namespace smf::scripting

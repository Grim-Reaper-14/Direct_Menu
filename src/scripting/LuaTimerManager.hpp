#pragma once

#include <sol/sol.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

class LuaTimerManager {
public:
    explicit LuaTimerManager(logging::LoggerApi& logger);

    std::uint64_t After(
        std::chrono::milliseconds delay,
        std::string owner,
        sol::protected_function callback);

    std::uint64_t Every(
        std::chrono::milliseconds interval,
        std::string owner,
        sol::protected_function callback);

    bool Cancel(std::uint64_t id);
    void RemoveByOwner(std::string_view owner);
    void Clear();
    void Update();

private:
    struct Timer {
        std::uint64_t id{};
        std::string owner;
        std::chrono::steady_clock::time_point next;
        std::chrono::milliseconds interval{};
        bool repeating{false};
        bool cancelled{false};
        sol::protected_function callback;
    };

    std::uint64_t Add(
        std::chrono::milliseconds delay,
        bool repeating,
        std::string owner,
        sol::protected_function callback);

    logging::LoggerApi& logger_;
    std::vector<Timer> timers_;
    std::uint64_t nextId_{1};
};

} // namespace smf::scripting

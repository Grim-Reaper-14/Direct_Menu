#pragma once

#include <sol/sol.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

class LuaEvents {
public:
    explicit LuaEvents(logging::LoggerApi& logger);

    std::uint64_t Subscribe(
        std::string eventName,
        std::string owner,
        sol::protected_function callback);

    bool Unsubscribe(std::uint64_t id);
    void RemoveByOwner(std::string_view owner);
    void Clear();

    void Emit(std::string_view eventName);

    template <typename... Args>
    void Emit(std::string_view eventName, Args&&... args) {
        const auto iterator = handlers_.find(std::string{eventName});
        if (iterator == handlers_.end()) {
            return;
        }

        const auto handlers = iterator->second;
        for (const Handler& handler : handlers) {
            if (!handler.callback.valid()) {
                continue;
            }

            const sol::protected_function_result result =
                handler.callback(std::forward<Args>(args)...);
            if (!result.valid()) {
                const sol::error error = result;
                ReportError(handler.owner, eventName, error.what());
            }
        }
    }

private:
    struct Handler {
        std::uint64_t id{};
        std::string owner;
        sol::protected_function callback;
    };

    void ReportError(
        std::string_view owner,
        std::string_view eventName,
        std::string_view message) const;

    logging::LoggerApi& logger_;
    std::unordered_map<std::string, std::vector<Handler>> handlers_;
    std::uint64_t nextId_{1};
};

} // namespace smf::scripting

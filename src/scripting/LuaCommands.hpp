#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smf::scripting {

class LuaCommands {
public:
    using Callback = std::function<void()>;

    struct CommandInfo {
        std::string name;
        std::string description;
        std::string owner;
    };

    bool Register(
        std::string name,
        std::string description,
        Callback callback,
        std::string owner = "__native__");

    bool Unregister(std::string_view name);
    void UnregisterByOwner(std::string_view owner);
    [[nodiscard]] bool Execute(std::string_view name) const;
    [[nodiscard]] bool Exists(std::string_view name) const;
    [[nodiscard]] std::vector<CommandInfo> List() const;

private:
    struct Entry {
        std::string description;
        std::string owner;
        Callback callback;
    };

    std::unordered_map<std::string, Entry> commands_;
};

} // namespace smf::scripting

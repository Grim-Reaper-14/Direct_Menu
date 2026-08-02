#pragma once

#include <sol/sol.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace smf::logging { class LoggerApi; }
namespace smf::scripting {

class LuaUI {
public:
    using OwnerChangedCallback = std::function<void(std::string_view)>;
    explicit LuaUI(logging::LoggerApi& logger, OwnerChangedCallback ownerChanged = {});
    std::uint64_t AddText(std::string owner, std::string text);
    std::uint64_t AddButton(std::string owner, std::string label, sol::protected_function callback);
    std::uint64_t AddCheckbox(std::string owner, std::string label, bool initialValue, sol::protected_function callback);
    bool Remove(std::uint64_t id);
    void RemoveByOwner(std::string_view owner);
    void Clear();
    void DrawInline();
    [[nodiscard]] bool Empty() const noexcept;
private:
    enum class WidgetType { Text, Button, Checkbox };
    struct Widget {
        std::uint64_t id{};
        std::string owner;
        WidgetType type{WidgetType::Text};
        std::string label;
        bool value{false};
        sol::protected_function callback;
    };
    logging::LoggerApi& logger_;
    OwnerChangedCallback ownerChanged_;
    std::vector<Widget> widgets_;
    std::uint64_t nextId_{1};
};

} // namespace smf::scripting

#include "scripting/LuaUI.hpp"

#include "logging/Logger.hpp"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <utility>

namespace smf::scripting {

LuaUI::LuaUI(logging::LoggerApi& logger)
    : logger_(logger) {
}

std::uint64_t LuaUI::AddText(std::string owner, std::string text) {
    const std::uint64_t id = nextId_++;
    widgets_.push_back(Widget{
        .id = id,
        .owner = std::move(owner),
        .type = WidgetType::Text,
        .label = std::move(text)
    });
    return id;
}

std::uint64_t LuaUI::AddButton(
    std::string owner,
    std::string label,
    sol::protected_function callback) {
    if (!callback.valid()) {
        return 0;
    }

    const std::uint64_t id = nextId_++;
    widgets_.push_back(Widget{
        .id = id,
        .owner = std::move(owner),
        .type = WidgetType::Button,
        .label = std::move(label),
        .callback = std::move(callback)
    });
    return id;
}

std::uint64_t LuaUI::AddCheckbox(
    std::string owner,
    std::string label,
    const bool initialValue,
    sol::protected_function callback) {
    const std::uint64_t id = nextId_++;
    widgets_.push_back(Widget{
        .id = id,
        .owner = std::move(owner),
        .type = WidgetType::Checkbox,
        .label = std::move(label),
        .value = initialValue,
        .callback = std::move(callback)
    });
    return id;
}

bool LuaUI::Remove(const std::uint64_t id) {
    const auto oldSize = widgets_.size();
    std::erase_if(widgets_, [id](const Widget& widget) {
        return widget.id == id;
    });
    return widgets_.size() != oldSize;
}

void LuaUI::RemoveByOwner(const std::string_view owner) {
    std::erase_if(widgets_, [owner](const Widget& widget) {
        return widget.owner == owner;
    });
}

void LuaUI::Clear() {
    widgets_.clear();
}

void LuaUI::DrawInline() {
    for (Widget& widget : widgets_) {
        ImGui::PushID(static_cast<int>(widget.id));

        switch (widget.type) {
        case WidgetType::Text:
            ImGui::TextUnformatted(widget.label.c_str());
            break;

        case WidgetType::Button:
            if (ImGui::Button(widget.label.c_str()) && widget.callback.valid()) {
                const sol::protected_function_result result = widget.callback();
                if (!result.valid()) {
                    const sol::error error = result;
                    logger_.Error(std::format(
                        "Lua UI button '{}' owned by '{}' failed: {}",
                        widget.label,
                        widget.owner,
                        error.what()));
                }
            }
            break;

        case WidgetType::Checkbox:
            if (ImGui::Checkbox(widget.label.c_str(), &widget.value) &&
                widget.callback.valid()) {
                const sol::protected_function_result result = widget.callback(widget.value);
                if (!result.valid()) {
                    const sol::error error = result;
                    logger_.Error(std::format(
                        "Lua UI checkbox '{}' owned by '{}' failed: {}",
                        widget.label,
                        widget.owner,
                        error.what()));
                }
            }
            break;
        }

        ImGui::PopID();
    }
}

bool LuaUI::Empty() const noexcept {
    return widgets_.empty();
}

} // namespace smf::scripting

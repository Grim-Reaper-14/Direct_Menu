#include "scripting/LuaImGuiBindings.hpp"

#include "logging/Logger.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace smf::scripting {
namespace {

struct BindingState {
    logging::LoggerApi& logger;
    ImGuiFrameScopeProvider frameScopeProvider;

    [[nodiscard]] bool CanDraw(const std::string_view functionName) const {
        if (frameScopeProvider && frameScopeProvider()) {
            return true;
        }
        logger.Warning(
            "Lua ImGui." + std::string{functionName} +
            " ignored outside an event.on('draw', ...) callback.");
        return false;
    }
};

template <typename Value>
void SetConstant(sol::table& table, const char* name, const Value value) {
    table[name] = static_cast<int>(value);
}

void RegisterConstants(sol::state& lua, sol::table& api) {
    sol::table windowFlags = lua.create_table();
    SetConstant(windowFlags, "None", ImGuiWindowFlags_None);
    SetConstant(windowFlags, "NoTitleBar", ImGuiWindowFlags_NoTitleBar);
    SetConstant(windowFlags, "NoResize", ImGuiWindowFlags_NoResize);
    SetConstant(windowFlags, "NoMove", ImGuiWindowFlags_NoMove);
    SetConstant(windowFlags, "NoScrollbar", ImGuiWindowFlags_NoScrollbar);
    SetConstant(windowFlags, "NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse);
    SetConstant(windowFlags, "NoCollapse", ImGuiWindowFlags_NoCollapse);
    SetConstant(windowFlags, "AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize);
    SetConstant(windowFlags, "NoBackground", ImGuiWindowFlags_NoBackground);
    SetConstant(windowFlags, "NoSavedSettings", ImGuiWindowFlags_NoSavedSettings);
    SetConstant(windowFlags, "NoMouseInputs", ImGuiWindowFlags_NoMouseInputs);
    SetConstant(windowFlags, "MenuBar", ImGuiWindowFlags_MenuBar);
    SetConstant(windowFlags, "HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar);
    SetConstant(windowFlags, "NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing);
    SetConstant(windowFlags, "NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus);
    SetConstant(windowFlags, "AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar);
    SetConstant(windowFlags, "AlwaysHorizontalScrollbar", ImGuiWindowFlags_AlwaysHorizontalScrollbar);
    SetConstant(windowFlags, "NoNavInputs", ImGuiWindowFlags_NoNavInputs);
    SetConstant(windowFlags, "NoNavFocus", ImGuiWindowFlags_NoNavFocus);
    SetConstant(windowFlags, "UnsavedDocument", ImGuiWindowFlags_UnsavedDocument);
    api["WindowFlags"] = std::move(windowFlags);

    sol::table conditions = lua.create_table();
    SetConstant(conditions, "None", ImGuiCond_None);
    SetConstant(conditions, "Always", ImGuiCond_Always);
    SetConstant(conditions, "Once", ImGuiCond_Once);
    SetConstant(conditions, "FirstUseEver", ImGuiCond_FirstUseEver);
    SetConstant(conditions, "Appearing", ImGuiCond_Appearing);
    api["Cond"] = std::move(conditions);

    sol::table inputTextFlags = lua.create_table();
    SetConstant(inputTextFlags, "None", ImGuiInputTextFlags_None);
    SetConstant(inputTextFlags, "CharsDecimal", ImGuiInputTextFlags_CharsDecimal);
    SetConstant(inputTextFlags, "CharsHexadecimal", ImGuiInputTextFlags_CharsHexadecimal);
    SetConstant(inputTextFlags, "CharsScientific", ImGuiInputTextFlags_CharsScientific);
    SetConstant(inputTextFlags, "CharsUppercase", ImGuiInputTextFlags_CharsUppercase);
    SetConstant(inputTextFlags, "CharsNoBlank", ImGuiInputTextFlags_CharsNoBlank);
    SetConstant(inputTextFlags, "EnterReturnsTrue", ImGuiInputTextFlags_EnterReturnsTrue);
    SetConstant(inputTextFlags, "EscapeClearsAll", ImGuiInputTextFlags_EscapeClearsAll);
    SetConstant(inputTextFlags, "ReadOnly", ImGuiInputTextFlags_ReadOnly);
    SetConstant(inputTextFlags, "Password", ImGuiInputTextFlags_Password);
    SetConstant(inputTextFlags, "AutoSelectAll", ImGuiInputTextFlags_AutoSelectAll);
    SetConstant(inputTextFlags, "NoHorizontalScroll", ImGuiInputTextFlags_NoHorizontalScroll);
    SetConstant(inputTextFlags, "NoUndoRedo", ImGuiInputTextFlags_NoUndoRedo);
    api["InputTextFlags"] = std::move(inputTextFlags);

    sol::table treeNodeFlags = lua.create_table();
    SetConstant(treeNodeFlags, "None", ImGuiTreeNodeFlags_None);
    SetConstant(treeNodeFlags, "Selected", ImGuiTreeNodeFlags_Selected);
    SetConstant(treeNodeFlags, "Framed", ImGuiTreeNodeFlags_Framed);
    SetConstant(treeNodeFlags, "DefaultOpen", ImGuiTreeNodeFlags_DefaultOpen);
    SetConstant(treeNodeFlags, "OpenOnDoubleClick", ImGuiTreeNodeFlags_OpenOnDoubleClick);
    SetConstant(treeNodeFlags, "OpenOnArrow", ImGuiTreeNodeFlags_OpenOnArrow);
    SetConstant(treeNodeFlags, "Leaf", ImGuiTreeNodeFlags_Leaf);
    SetConstant(treeNodeFlags, "Bullet", ImGuiTreeNodeFlags_Bullet);
    SetConstant(treeNodeFlags, "SpanAvailWidth", ImGuiTreeNodeFlags_SpanAvailWidth);
    SetConstant(treeNodeFlags, "SpanFullWidth", ImGuiTreeNodeFlags_SpanFullWidth);
    api["TreeNodeFlags"] = std::move(treeNodeFlags);

    sol::table selectableFlags = lua.create_table();
    SetConstant(selectableFlags, "None", ImGuiSelectableFlags_None);
    SetConstant(selectableFlags, "NoAutoClosePopups", ImGuiSelectableFlags_NoAutoClosePopups);
    SetConstant(selectableFlags, "SpanAllColumns", ImGuiSelectableFlags_SpanAllColumns);
    SetConstant(selectableFlags, "AllowDoubleClick", ImGuiSelectableFlags_AllowDoubleClick);
    SetConstant(selectableFlags, "Disabled", ImGuiSelectableFlags_Disabled);
    SetConstant(selectableFlags, "AllowOverlap", ImGuiSelectableFlags_AllowOverlap);
    api["SelectableFlags"] = std::move(selectableFlags);

    sol::table tabBarFlags = lua.create_table();
    SetConstant(tabBarFlags, "None", ImGuiTabBarFlags_None);
    SetConstant(tabBarFlags, "Reorderable", ImGuiTabBarFlags_Reorderable);
    SetConstant(tabBarFlags, "AutoSelectNewTabs", ImGuiTabBarFlags_AutoSelectNewTabs);
    SetConstant(tabBarFlags, "TabListPopupButton", ImGuiTabBarFlags_TabListPopupButton);
    SetConstant(tabBarFlags, "NoTooltip", ImGuiTabBarFlags_NoTooltip);
    SetConstant(tabBarFlags, "FittingPolicyResizeDown", ImGuiTabBarFlags_FittingPolicyResizeDown);
    SetConstant(tabBarFlags, "FittingPolicyScroll", ImGuiTabBarFlags_FittingPolicyScroll);
    api["TabBarFlags"] = std::move(tabBarFlags);

    sol::table tabItemFlags = lua.create_table();
    SetConstant(tabItemFlags, "None", ImGuiTabItemFlags_None);
    SetConstant(tabItemFlags, "UnsavedDocument", ImGuiTabItemFlags_UnsavedDocument);
    SetConstant(tabItemFlags, "SetSelected", ImGuiTabItemFlags_SetSelected);
    SetConstant(tabItemFlags, "NoCloseWithMiddleMouseButton", ImGuiTabItemFlags_NoCloseWithMiddleMouseButton);
    SetConstant(tabItemFlags, "NoPushId", ImGuiTabItemFlags_NoPushId);
    SetConstant(tabItemFlags, "NoTooltip", ImGuiTabItemFlags_NoTooltip);
    SetConstant(tabItemFlags, "NoReorder", ImGuiTabItemFlags_NoReorder);
    SetConstant(tabItemFlags, "Leading", ImGuiTabItemFlags_Leading);
    SetConstant(tabItemFlags, "Trailing", ImGuiTabItemFlags_Trailing);
    api["TabItemFlags"] = std::move(tabItemFlags);

    sol::table hoveredFlags = lua.create_table();
    SetConstant(hoveredFlags, "None", ImGuiHoveredFlags_None);
    SetConstant(hoveredFlags, "AllowWhenBlockedByPopup", ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    SetConstant(hoveredFlags, "AllowWhenBlockedByActiveItem", ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    SetConstant(hoveredFlags, "AllowWhenDisabled", ImGuiHoveredFlags_AllowWhenDisabled);
    SetConstant(hoveredFlags, "ForTooltip", ImGuiHoveredFlags_ForTooltip);
    SetConstant(hoveredFlags, "Stationary", ImGuiHoveredFlags_Stationary);
    SetConstant(hoveredFlags, "DelayShort", ImGuiHoveredFlags_DelayShort);
    SetConstant(hoveredFlags, "DelayNormal", ImGuiHoveredFlags_DelayNormal);
    api["HoveredFlags"] = std::move(hoveredFlags);

    sol::table mouseButton = lua.create_table();
    SetConstant(mouseButton, "Left", ImGuiMouseButton_Left);
    SetConstant(mouseButton, "Right", ImGuiMouseButton_Right);
    SetConstant(mouseButton, "Middle", ImGuiMouseButton_Middle);
    api["MouseButton"] = std::move(mouseButton);
}

} // namespace

void RegisterLuaImGuiBindings(
    logging::LoggerApi& logger,
    sol::state& lua,
    ImGuiFrameScopeProvider frameScopeProvider) {
    auto state = std::make_shared<BindingState>(BindingState{
        .logger = logger,
        .frameScopeProvider = std::move(frameScopeProvider)});

    sol::table api = lua.create_named_table("ImGui");
    api["API_VERSION"] = "1.0.0";
    RegisterConstants(lua, api);

    api.set_function("Begin", [state](const std::string& name, sol::optional<int> flags) {
        return state->CanDraw("Begin") &&
               ImGui::Begin(name.c_str(), nullptr, flags.value_or(ImGuiWindowFlags_None));
    });
    api.set_function("End", [state]() {
        if (state->CanDraw("End")) {
            ImGui::End();
        }
    });
    api.set_function(
        "SetNextWindowPos",
        [state](const float x, const float y, sol::optional<int> condition) {
            if (state->CanDraw("SetNextWindowPos")) {
                ImGui::SetNextWindowPos(
                    {x, y},
                    condition.value_or(ImGuiCond_None));
            }
        });
    api.set_function(
        "SetNextWindowSize",
        [state](const float width, const float height, sol::optional<int> condition) {
            if (state->CanDraw("SetNextWindowSize")) {
                ImGui::SetNextWindowSize(
                    {width, height},
                    condition.value_or(ImGuiCond_None));
            }
        });
    api.set_function("SetNextWindowBgAlpha", [state](const float alpha) {
        if (state->CanDraw("SetNextWindowBgAlpha")) {
            ImGui::SetNextWindowBgAlpha(alpha);
        }
    });
    api.set_function("GetWindowPos", [state]() {
        if (!state->CanDraw("GetWindowPos")) {
            return std::tuple{0.0F, 0.0F};
        }
        const ImVec2 value = ImGui::GetWindowPos();
        return std::tuple{value.x, value.y};
    });
    api.set_function("GetWindowSize", [state]() {
        if (!state->CanDraw("GetWindowSize")) {
            return std::tuple{0.0F, 0.0F};
        }
        const ImVec2 value = ImGui::GetWindowSize();
        return std::tuple{value.x, value.y};
    });
    api.set_function("GetContentRegionAvail", [state]() {
        if (!state->CanDraw("GetContentRegionAvail")) {
            return std::tuple{0.0F, 0.0F};
        }
        const ImVec2 value = ImGui::GetContentRegionAvail();
        return std::tuple{value.x, value.y};
    });

    api.set_function("Text", [state](const std::string& text) {
        if (state->CanDraw("Text")) {
            ImGui::TextUnformatted(text.c_str());
        }
    });
    api.set_function("TextWrapped", [state](const std::string& text) {
        if (state->CanDraw("TextWrapped")) {
            ImGui::TextWrapped("%s", text.c_str());
        }
    });
    api.set_function(
        "TextColored",
        [state](
            const float red,
            const float green,
            const float blue,
            const float alpha,
            const std::string& text) {
            if (state->CanDraw("TextColored")) {
                ImGui::TextColored({red, green, blue, alpha}, "%s", text.c_str());
            }
        });

    api.set_function(
        "Button",
        [state](
            const std::string& label,
            sol::optional<float> width,
            sol::optional<float> height) {
            return state->CanDraw("Button") && ImGui::Button(
                label.c_str(),
                {width.value_or(0.0F), height.value_or(0.0F)});
        });
    api.set_function("SmallButton", [state](const std::string& label) {
        return state->CanDraw("SmallButton") && ImGui::SmallButton(label.c_str());
    });
    api.set_function("Checkbox", [state](const std::string& label, bool value) {
        const bool changed =
            state->CanDraw("Checkbox") && ImGui::Checkbox(label.c_str(), &value);
        return std::tuple{changed, value};
    });
    api.set_function("RadioButton", [state](const std::string& label, const bool active) {
        return state->CanDraw("RadioButton") && ImGui::RadioButton(label.c_str(), active);
    });
    api.set_function(
        "SliderFloat",
        [state](
            const std::string& label,
            float value,
            const float minimum,
            const float maximum,
            sol::optional<std::string> format) {
            const std::string displayFormat = format.value_or("%.3f");
            const bool changed = state->CanDraw("SliderFloat") && ImGui::SliderFloat(
                label.c_str(),
                &value,
                minimum,
                maximum,
                displayFormat.c_str());
            return std::tuple{changed, value};
        });
    api.set_function(
        "SliderInt",
        [state](
            const std::string& label,
            int value,
            const int minimum,
            const int maximum,
            sol::optional<std::string> format) {
            const std::string displayFormat = format.value_or("%d");
            const bool changed = state->CanDraw("SliderInt") && ImGui::SliderInt(
                label.c_str(),
                &value,
                minimum,
                maximum,
                displayFormat.c_str());
            return std::tuple{changed, value};
        });
    api.set_function(
        "InputFloat",
        [state](const std::string& label, float value, sol::optional<float> step) {
            const bool changed = state->CanDraw("InputFloat") && ImGui::InputFloat(
                label.c_str(),
                &value,
                step.value_or(0.0F));
            return std::tuple{changed, value};
        });
    api.set_function(
        "InputInt",
        [state](const std::string& label, int value, sol::optional<int> step) {
            const bool changed = state->CanDraw("InputInt") && ImGui::InputInt(
                label.c_str(),
                &value,
                step.value_or(1));
            return std::tuple{changed, value};
        });
    api.set_function(
        "InputText",
        [state](
            const std::string& label,
            const std::string& value,
            sol::optional<int> capacity,
            sol::optional<int> flags) {
            const std::size_t bufferSize = std::clamp<std::size_t>(
                std::max<std::size_t>(
                    value.size() + 1,
                    static_cast<std::size_t>(std::max(capacity.value_or(256), 2))),
                2,
                4096);
            std::array<char, 4096> buffer{};
            const std::size_t count = std::min(value.size(), bufferSize - 1);
            std::memcpy(buffer.data(), value.data(), count);
            const bool changed = state->CanDraw("InputText") && ImGui::InputText(
                label.c_str(),
                buffer.data(),
                bufferSize,
                flags.value_or(ImGuiInputTextFlags_None));
            return std::tuple{changed, std::string{buffer.data()}};
        });
    api.set_function(
        "ColorEdit4",
        [state](
            const std::string& label,
            float red,
            float green,
            float blue,
            float alpha) {
            float color[4] = {red, green, blue, alpha};
            const bool changed =
                state->CanDraw("ColorEdit4") && ImGui::ColorEdit4(label.c_str(), color);
            return std::tuple{changed, color[0], color[1], color[2], color[3]};
        });
    api.set_function(
        "ProgressBar",
        [state](
            const float fraction,
            sol::optional<float> width,
            sol::optional<float> height,
            sol::optional<std::string> overlay) {
            if (state->CanDraw("ProgressBar")) {
                const std::string overlayText = overlay.value_or("");
                ImGui::ProgressBar(
                    fraction,
                    {width.value_or(-1.0F), height.value_or(0.0F)},
                    overlayText.empty() ? nullptr : overlayText.c_str());
            }
        });

    api.set_function("Separator", [state]() {
        if (state->CanDraw("Separator")) {
            ImGui::Separator();
        }
    });
    api.set_function("Spacing", [state]() {
        if (state->CanDraw("Spacing")) {
            ImGui::Spacing();
        }
    });
    api.set_function("NewLine", [state]() {
        if (state->CanDraw("NewLine")) {
            ImGui::NewLine();
        }
    });
    api.set_function(
        "SameLine",
        [state](sol::optional<float> offset, sol::optional<float> spacing) {
            if (state->CanDraw("SameLine")) {
                ImGui::SameLine(offset.value_or(0.0F), spacing.value_or(-1.0F));
            }
        });
    api.set_function("Dummy", [state](const float width, const float height) {
        if (state->CanDraw("Dummy")) {
            ImGui::Dummy({width, height});
        }
    });
    api.set_function("Indent", [state](sol::optional<float> width) {
        if (state->CanDraw("Indent")) {
            ImGui::Indent(width.value_or(0.0F));
        }
    });
    api.set_function("Unindent", [state](sol::optional<float> width) {
        if (state->CanDraw("Unindent")) {
            ImGui::Unindent(width.value_or(0.0F));
        }
    });
    api.set_function("BeginGroup", [state]() {
        if (state->CanDraw("BeginGroup")) {
            ImGui::BeginGroup();
        }
    });
    api.set_function("EndGroup", [state]() {
        if (state->CanDraw("EndGroup")) {
            ImGui::EndGroup();
        }
    });
    api.set_function("BeginDisabled", [state](sol::optional<bool> disabled) {
        if (state->CanDraw("BeginDisabled")) {
            ImGui::BeginDisabled(disabled.value_or(true));
        }
    });
    api.set_function("EndDisabled", [state]() {
        if (state->CanDraw("EndDisabled")) {
            ImGui::EndDisabled();
        }
    });

    api.set_function(
        "TreeNode",
        [state](const std::string& label, sol::optional<int> flags) {
            return state->CanDraw("TreeNode") && ImGui::TreeNodeEx(
                label.c_str(),
                flags.value_or(ImGuiTreeNodeFlags_None));
        });
    api.set_function("TreePop", [state]() {
        if (state->CanDraw("TreePop")) {
            ImGui::TreePop();
        }
    });
    api.set_function(
        "CollapsingHeader",
        [state](const std::string& label, sol::optional<int> flags) {
            return state->CanDraw("CollapsingHeader") && ImGui::CollapsingHeader(
                label.c_str(),
                flags.value_or(ImGuiTreeNodeFlags_None));
        });
    api.set_function(
        "Selectable",
        [state](
            const std::string& label,
            sol::optional<bool> selected,
            sol::optional<int> flags,
            sol::optional<float> width,
            sol::optional<float> height) {
            return state->CanDraw("Selectable") && ImGui::Selectable(
                label.c_str(),
                selected.value_or(false),
                flags.value_or(ImGuiSelectableFlags_None),
                {width.value_or(0.0F), height.value_or(0.0F)});
        });

    api.set_function(
        "BeginTabBar",
        [state](const std::string& id, sol::optional<int> flags) {
            return state->CanDraw("BeginTabBar") && ImGui::BeginTabBar(
                id.c_str(),
                flags.value_or(ImGuiTabBarFlags_None));
        });
    api.set_function("EndTabBar", [state]() {
        if (state->CanDraw("EndTabBar")) {
            ImGui::EndTabBar();
        }
    });
    api.set_function(
        "BeginTabItem",
        [state](const std::string& label, sol::optional<int> flags) {
            return state->CanDraw("BeginTabItem") && ImGui::BeginTabItem(
                label.c_str(),
                nullptr,
                flags.value_or(ImGuiTabItemFlags_None));
        });
    api.set_function("EndTabItem", [state]() {
        if (state->CanDraw("EndTabItem")) {
            ImGui::EndTabItem();
        }
    });
    api.set_function(
        "BeginCombo",
        [state](
            const std::string& label,
            const std::string& preview,
            sol::optional<int> flags) {
            return state->CanDraw("BeginCombo") && ImGui::BeginCombo(
                label.c_str(),
                preview.c_str(),
                flags.value_or(ImGuiComboFlags_None));
        });
    api.set_function("EndCombo", [state]() {
        if (state->CanDraw("EndCombo")) {
            ImGui::EndCombo();
        }
    });

    api.set_function("IsItemHovered", [state](sol::optional<int> flags) {
        return state->CanDraw("IsItemHovered") &&
               ImGui::IsItemHovered(flags.value_or(ImGuiHoveredFlags_None));
    });
    api.set_function("IsItemActive", [state]() {
        return state->CanDraw("IsItemActive") && ImGui::IsItemActive();
    });
    api.set_function("IsItemEdited", [state]() {
        return state->CanDraw("IsItemEdited") && ImGui::IsItemEdited();
    });
    api.set_function("IsItemClicked", [state](sol::optional<int> button) {
        return state->CanDraw("IsItemClicked") &&
               ImGui::IsItemClicked(button.value_or(ImGuiMouseButton_Left));
    });
    api.set_function("SetTooltip", [state](const std::string& text) {
        if (state->CanDraw("SetTooltip")) {
            ImGui::SetTooltip("%s", text.c_str());
        }
    });
    api.set_function("PushID", [state](const std::string& id) {
        if (state->CanDraw("PushID")) {
            ImGui::PushID(id.c_str());
        }
    });
    api.set_function("PopID", [state]() {
        if (state->CanDraw("PopID")) {
            ImGui::PopID();
        }
    });
    api.set_function("SetNextItemWidth", [state](const float width) {
        if (state->CanDraw("SetNextItemWidth")) {
            ImGui::SetNextItemWidth(width);
        }
    });
    api.set_function("PushItemWidth", [state](const float width) {
        if (state->CanDraw("PushItemWidth")) {
            ImGui::PushItemWidth(width);
        }
    });
    api.set_function("PopItemWidth", [state]() {
        if (state->CanDraw("PopItemWidth")) {
            ImGui::PopItemWidth();
        }
    });

    lua["imgui"] = api;
}

} // namespace smf::scripting

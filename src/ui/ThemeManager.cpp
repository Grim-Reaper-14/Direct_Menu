#include "ui/ThemeManager.hpp"

#include <algorithm>

namespace smf::ui {
namespace {

void ApplyCommonShape(ImGuiStyle& style) {
    style.WindowRounding = 8.0F;
    style.ChildRounding = 7.0F;
    style.FrameRounding = 5.0F;
    style.PopupRounding = 6.0F;
    style.ScrollbarRounding = 8.0F;
    style.GrabRounding = 4.0F;
    style.TabRounding = 5.0F;
    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 0.0F;
    style.WindowPadding = {14.0F, 12.0F};
    style.FramePadding = {10.0F, 7.0F};
    style.ItemSpacing = {8.0F, 7.0F};
}

void ApplyAccent(ImGuiStyle& style, const ImVec4 accent) {
    ImVec4 hovered = accent;
    hovered.x = std::min(hovered.x + 0.10F, 1.0F);
    hovered.y = std::min(hovered.y + 0.10F, 1.0F);
    hovered.z = std::min(hovered.z + 0.10F, 1.0F);

    ImVec4 active = accent;
    active.x = std::max(active.x - 0.08F, 0.0F);
    active.y = std::max(active.y - 0.08F, 0.0F);
    active.z = std::max(active.z - 0.08F, 0.0F);

    auto& colors = style.Colors;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = hovered;

    colors[ImGuiCol_Button] = ImVec4{accent.x, accent.y, accent.z, 0.22F};
    colors[ImGuiCol_ButtonHovered] = ImVec4{accent.x, accent.y, accent.z, 0.62F};
    colors[ImGuiCol_ButtonActive] = ImVec4{active.x, active.y, active.z, 0.92F};

    colors[ImGuiCol_Header] = ImVec4{accent.x, accent.y, accent.z, 0.35F};
    colors[ImGuiCol_HeaderHovered] = ImVec4{accent.x, accent.y, accent.z, 0.58F};
    colors[ImGuiCol_HeaderActive] = ImVec4{accent.x, accent.y, accent.z, 0.75F};

    colors[ImGuiCol_FrameBgHovered] = ImVec4{accent.x, accent.y, accent.z, 0.20F};
    colors[ImGuiCol_FrameBgActive] = ImVec4{accent.x, accent.y, accent.z, 0.30F};

    colors[ImGuiCol_Separator] = ImVec4{accent.x, accent.y, accent.z, 0.38F};
    colors[ImGuiCol_SeparatorHovered] = ImVec4{accent.x, accent.y, accent.z, 0.72F};
    colors[ImGuiCol_SeparatorActive] = accent;
    colors[ImGuiCol_Border] = ImVec4{accent.x, accent.y, accent.z, 0.45F};

    colors[ImGuiCol_ScrollbarGrab] = ImVec4{accent.x, accent.y, accent.z, 0.28F};
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{accent.x, accent.y, accent.z, 0.48F};
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{accent.x, accent.y, accent.z, 0.68F};

    colors[ImGuiCol_ResizeGrip] = ImVec4{accent.x, accent.y, accent.z, 0.22F};
    colors[ImGuiCol_ResizeGripHovered] = ImVec4{accent.x, accent.y, accent.z, 0.67F};
    colors[ImGuiCol_ResizeGripActive] = accent;
    colors[ImGuiCol_NavHighlight] = accent;
}

} // namespace

ThemeManager::ThemeManager()
    : themes_{
          {"Midnight", "Midnight Blue", {0.18F, 0.65F, 0.95F, 1.0F}},
          {"Slate", "Classic Slate", {0.48F, 0.67F, 0.76F, 1.0F}},
          {"Purple", "Purple Neon", {0.69F, 0.38F, 0.95F, 1.0F}},
          {"Crimson", "Crimson", {0.90F, 0.24F, 0.31F, 1.0F}},
          {"Light", "Light", {0.12F, 0.48F, 0.88F, 1.0F}}} {
}

bool ThemeManager::Apply(const std::string_view id) {
    const auto found = std::ranges::find_if(
        themes_,
        [id](const ThemeEntry& theme) {
            return theme.id == id;
        });
    if (found == themes_.end()) {
        return false;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    if (found->id == "Light") {
        ImGui::StyleColorsLight(&style);
    } else if (found->id == "Slate") {
        ImGui::StyleColorsClassic(&style);
    } else {
        ImGui::StyleColorsDark(&style);
    }

    if (found->id == "Midnight") {
        style.Colors[ImGuiCol_WindowBg] = {0.035F, 0.050F, 0.075F, 1.0F};
        style.Colors[ImGuiCol_ChildBg] = {0.050F, 0.070F, 0.100F, 1.0F};
        style.Colors[ImGuiCol_PopupBg] = {0.045F, 0.060F, 0.090F, 0.98F};
        style.Colors[ImGuiCol_Border] = {0.15F, 0.26F, 0.38F, 0.75F};
        style.Colors[ImGuiCol_FrameBg] = {0.075F, 0.105F, 0.145F, 1.0F};
        style.Colors[ImGuiCol_FrameBgHovered] = {0.10F, 0.15F, 0.21F, 1.0F};
    } else if (found->id == "Purple") {
        style.Colors[ImGuiCol_WindowBg] = {0.055F, 0.035F, 0.075F, 1.0F};
        style.Colors[ImGuiCol_ChildBg] = {0.075F, 0.045F, 0.100F, 1.0F};
        style.Colors[ImGuiCol_Border] = {0.31F, 0.16F, 0.43F, 0.85F};
        style.Colors[ImGuiCol_FrameBg] = {0.12F, 0.07F, 0.16F, 1.0F};
    } else if (found->id == "Crimson") {
        style.Colors[ImGuiCol_WindowBg] = {0.070F, 0.035F, 0.040F, 1.0F};
        style.Colors[ImGuiCol_ChildBg] = {0.095F, 0.045F, 0.050F, 1.0F};
        style.Colors[ImGuiCol_Border] = {0.42F, 0.14F, 0.16F, 0.85F};
        style.Colors[ImGuiCol_FrameBg] = {0.15F, 0.07F, 0.08F, 1.0F};
    }

    ApplyCommonShape(style);
    ApplyAccent(style, found->accent);
    current_ = found->id;
    accent_ = found->accent;
    return true;
}

const std::vector<ThemeEntry>& ThemeManager::Themes() const noexcept {
    return themes_;
}

std::string_view ThemeManager::Current() const noexcept {
    return current_;
}

ImVec4 ThemeManager::Accent() const noexcept {
    return accent_;
}

} // namespace smf::ui

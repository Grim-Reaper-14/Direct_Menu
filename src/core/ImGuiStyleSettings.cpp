#include "core/ImGuiStyleSettings.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <string>

namespace smf::core {
namespace {

bool ParseFloat(const std::string_view text, float& value) {
    try {
        std::size_t consumed = 0;
        const float parsed = std::stof(std::string{text}, &consumed);
        if (consumed != text.size() || !std::isfinite(parsed)) return false;
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseInteger(const std::string_view text, int& value) {
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
}

bool ParseBoolean(const std::string_view text, bool& value) {
    if (text == "1" || text == "true" || text == "on") {
        value = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "off") {
        value = false;
        return true;
    }
    return false;
}

bool ParseVector2(const std::string_view text, ImVec2& value) {
    const std::size_t separator = text.find(',');
    return separator != std::string_view::npos &&
           ParseFloat(text.substr(0, separator), value.x) &&
           ParseFloat(text.substr(separator + 1), value.y);
}

bool ParseVector4(const std::string_view text, ImVec4& value) {
    const std::size_t first = text.find(',');
    if (first == std::string_view::npos) return false;
    const std::size_t second = text.find(',', first + 1);
    if (second == std::string_view::npos) return false;
    const std::size_t third = text.find(',', second + 1);
    if (third == std::string_view::npos) return false;

    return ParseFloat(text.substr(0, first), value.x) &&
           ParseFloat(text.substr(first + 1, second - first - 1), value.y) &&
           ParseFloat(text.substr(second + 1, third - second - 1), value.z) &&
           ParseFloat(text.substr(third + 1), value.w);
}

void WriteFloat(
    std::ostream& output,
    const std::string_view key,
    const float value) {
    output << "imgui." << key << '=' << std::defaultfloat
           << std::setprecision(std::numeric_limits<float>::max_digits10)
           << value << '\n';
}

void WriteInteger(
    std::ostream& output,
    const std::string_view key,
    const int value) {
    output << "imgui." << key << '=' << value << '\n';
}

void WriteBoolean(
    std::ostream& output,
    const std::string_view key,
    const bool value) {
    output << "imgui." << key << '=' << (value ? 1 : 0) << '\n';
}

void WriteVector2(
    std::ostream& output,
    const std::string_view key,
    const ImVec2 value) {
    output << "imgui." << key << '=' << std::defaultfloat
           << std::setprecision(std::numeric_limits<float>::max_digits10)
           << value.x << ',' << value.y << '\n';
}

void WriteVector4(
    std::ostream& output,
    const std::string_view key,
    const ImVec4 value) {
    output << "imgui." << key << '=' << std::defaultfloat
           << std::setprecision(std::numeric_limits<float>::max_digits10)
           << value.x << ',' << value.y << ',' << value.z << ',' << value.w
           << '\n';
}

} // namespace

void ImGuiStyleSettings::Capture(const ImGuiStyle& style) {
    style_ = style;
    hasValue_ = true;
}

void ImGuiStyleSettings::Apply(ImGuiStyle& style) const {
    if (hasValue_) style = style_;
}

void ImGuiStyleSettings::Reset() noexcept {
    style_ = ImGuiStyle{};
    hasValue_ = false;
}

bool ImGuiStyleSettings::HasValue() const noexcept { return hasValue_; }

bool ImGuiStyleSettings::SetValue(
    const std::string_view key,
    const std::string_view value) {
#define STYLE_FLOAT(name, member) \
    if (key == name) { \
        if (!ParseFloat(value, style_.member)) return false; \
        hasValue_ = true; \
        return true; \
    }
#define STYLE_VEC2(name, member) \
    if (key == name) { \
        if (!ParseVector2(value, style_.member)) return false; \
        hasValue_ = true; \
        return true; \
    }
#define STYLE_BOOL(name, member) \
    if (key == name) { \
        if (!ParseBoolean(value, style_.member)) return false; \
        hasValue_ = true; \
        return true; \
    }
#define STYLE_INT(name, member, type) \
    if (key == name) { \
        int parsed = 0; \
        if (!ParseInteger(value, parsed)) return false; \
        style_.member = static_cast<type>(parsed); \
        hasValue_ = true; \
        return true; \
    }

    STYLE_FLOAT("alpha", Alpha)
    STYLE_FLOAT("disabled_alpha", DisabledAlpha)
    STYLE_VEC2("window_padding", WindowPadding)
    STYLE_FLOAT("window_rounding", WindowRounding)
    STYLE_FLOAT("window_border_size", WindowBorderSize)
    STYLE_FLOAT("window_border_hover_padding", WindowBorderHoverPadding)
    STYLE_VEC2("window_min_size", WindowMinSize)
    STYLE_VEC2("window_title_align", WindowTitleAlign)
    STYLE_INT("window_menu_button_position", WindowMenuButtonPosition, ImGuiDir)
    STYLE_FLOAT("child_rounding", ChildRounding)
    STYLE_FLOAT("child_border_size", ChildBorderSize)
    STYLE_FLOAT("popup_rounding", PopupRounding)
    STYLE_FLOAT("popup_border_size", PopupBorderSize)
    STYLE_VEC2("frame_padding", FramePadding)
    STYLE_FLOAT("frame_rounding", FrameRounding)
    STYLE_FLOAT("frame_border_size", FrameBorderSize)
    STYLE_VEC2("item_spacing", ItemSpacing)
    STYLE_VEC2("item_inner_spacing", ItemInnerSpacing)
    STYLE_VEC2("cell_padding", CellPadding)
    STYLE_VEC2("touch_extra_padding", TouchExtraPadding)
    STYLE_FLOAT("indent_spacing", IndentSpacing)
    STYLE_FLOAT("columns_min_spacing", ColumnsMinSpacing)
    STYLE_FLOAT("scrollbar_size", ScrollbarSize)
    STYLE_FLOAT("scrollbar_rounding", ScrollbarRounding)
    STYLE_FLOAT("grab_min_size", GrabMinSize)
    STYLE_FLOAT("grab_rounding", GrabRounding)
    STYLE_FLOAT("log_slider_deadzone", LogSliderDeadzone)
    STYLE_FLOAT("image_border_size", ImageBorderSize)
    STYLE_FLOAT("tab_rounding", TabRounding)
    STYLE_FLOAT("tab_border_size", TabBorderSize)
    STYLE_FLOAT("tab_close_button_min_width_selected", TabCloseButtonMinWidthSelected)
    STYLE_FLOAT("tab_close_button_min_width_unselected", TabCloseButtonMinWidthUnselected)
    STYLE_FLOAT("tab_bar_border_size", TabBarBorderSize)
    STYLE_FLOAT("tab_bar_overline_size", TabBarOverlineSize)
    STYLE_FLOAT("table_angled_headers_angle", TableAngledHeadersAngle)
    STYLE_VEC2("table_angled_headers_text_align", TableAngledHeadersTextAlign)
    STYLE_INT("color_button_position", ColorButtonPosition, ImGuiDir)
    STYLE_VEC2("button_text_align", ButtonTextAlign)
    STYLE_VEC2("selectable_text_align", SelectableTextAlign)
    STYLE_FLOAT("separator_text_border_size", SeparatorTextBorderSize)
    STYLE_VEC2("separator_text_align", SeparatorTextAlign)
    STYLE_VEC2("separator_text_padding", SeparatorTextPadding)
    STYLE_VEC2("display_window_padding", DisplayWindowPadding)
    STYLE_VEC2("display_safe_area_padding", DisplaySafeAreaPadding)
    STYLE_FLOAT("mouse_cursor_scale", MouseCursorScale)
    STYLE_BOOL("anti_aliased_lines", AntiAliasedLines)
    STYLE_BOOL("anti_aliased_lines_use_tex", AntiAliasedLinesUseTex)
    STYLE_BOOL("anti_aliased_fill", AntiAliasedFill)
    STYLE_FLOAT("curve_tessellation_tol", CurveTessellationTol)
    STYLE_FLOAT("circle_tessellation_max_error", CircleTessellationMaxError)
    STYLE_FLOAT("hover_stationary_delay", HoverStationaryDelay)
    STYLE_FLOAT("hover_delay_short", HoverDelayShort)
    STYLE_FLOAT("hover_delay_normal", HoverDelayNormal)
    STYLE_INT("hover_flags_tooltip_mouse", HoverFlagsForTooltipMouse, ImGuiHoveredFlags)
    STYLE_INT("hover_flags_tooltip_nav", HoverFlagsForTooltipNav, ImGuiHoveredFlags)

#undef STYLE_FLOAT
#undef STYLE_VEC2
#undef STYLE_BOOL
#undef STYLE_INT

    constexpr std::string_view colorPrefix{"color."};
    if (key.starts_with(colorPrefix)) {
        int index = -1;
        if (!ParseInteger(key.substr(colorPrefix.size()), index) ||
            index < 0 || index >= ImGuiCol_COUNT ||
            !ParseVector4(value, style_.Colors[index])) {
            return false;
        }
        hasValue_ = true;
        return true;
    }
    return false;
}

void ImGuiStyleSettings::Write(std::ostream& output) const {
    if (!hasValue_) return;

    WriteFloat(output, "alpha", style_.Alpha);
    WriteFloat(output, "disabled_alpha", style_.DisabledAlpha);
    WriteVector2(output, "window_padding", style_.WindowPadding);
    WriteFloat(output, "window_rounding", style_.WindowRounding);
    WriteFloat(output, "window_border_size", style_.WindowBorderSize);
    WriteFloat(output, "window_border_hover_padding", style_.WindowBorderHoverPadding);
    WriteVector2(output, "window_min_size", style_.WindowMinSize);
    WriteVector2(output, "window_title_align", style_.WindowTitleAlign);
    WriteInteger(output, "window_menu_button_position", style_.WindowMenuButtonPosition);
    WriteFloat(output, "child_rounding", style_.ChildRounding);
    WriteFloat(output, "child_border_size", style_.ChildBorderSize);
    WriteFloat(output, "popup_rounding", style_.PopupRounding);
    WriteFloat(output, "popup_border_size", style_.PopupBorderSize);
    WriteVector2(output, "frame_padding", style_.FramePadding);
    WriteFloat(output, "frame_rounding", style_.FrameRounding);
    WriteFloat(output, "frame_border_size", style_.FrameBorderSize);
    WriteVector2(output, "item_spacing", style_.ItemSpacing);
    WriteVector2(output, "item_inner_spacing", style_.ItemInnerSpacing);
    WriteVector2(output, "cell_padding", style_.CellPadding);
    WriteVector2(output, "touch_extra_padding", style_.TouchExtraPadding);
    WriteFloat(output, "indent_spacing", style_.IndentSpacing);
    WriteFloat(output, "columns_min_spacing", style_.ColumnsMinSpacing);
    WriteFloat(output, "scrollbar_size", style_.ScrollbarSize);
    WriteFloat(output, "scrollbar_rounding", style_.ScrollbarRounding);
    WriteFloat(output, "grab_min_size", style_.GrabMinSize);
    WriteFloat(output, "grab_rounding", style_.GrabRounding);
    WriteFloat(output, "log_slider_deadzone", style_.LogSliderDeadzone);
    WriteFloat(output, "image_border_size", style_.ImageBorderSize);
    WriteFloat(output, "tab_rounding", style_.TabRounding);
    WriteFloat(output, "tab_border_size", style_.TabBorderSize);
    WriteFloat(output, "tab_close_button_min_width_selected", style_.TabCloseButtonMinWidthSelected);
    WriteFloat(output, "tab_close_button_min_width_unselected", style_.TabCloseButtonMinWidthUnselected);
    WriteFloat(output, "tab_bar_border_size", style_.TabBarBorderSize);
    WriteFloat(output, "tab_bar_overline_size", style_.TabBarOverlineSize);
    WriteFloat(output, "table_angled_headers_angle", style_.TableAngledHeadersAngle);
    WriteVector2(output, "table_angled_headers_text_align", style_.TableAngledHeadersTextAlign);
    WriteInteger(output, "color_button_position", style_.ColorButtonPosition);
    WriteVector2(output, "button_text_align", style_.ButtonTextAlign);
    WriteVector2(output, "selectable_text_align", style_.SelectableTextAlign);
    WriteFloat(output, "separator_text_border_size", style_.SeparatorTextBorderSize);
    WriteVector2(output, "separator_text_align", style_.SeparatorTextAlign);
    WriteVector2(output, "separator_text_padding", style_.SeparatorTextPadding);
    WriteVector2(output, "display_window_padding", style_.DisplayWindowPadding);
    WriteVector2(output, "display_safe_area_padding", style_.DisplaySafeAreaPadding);
    WriteFloat(output, "mouse_cursor_scale", style_.MouseCursorScale);
    WriteBoolean(output, "anti_aliased_lines", style_.AntiAliasedLines);
    WriteBoolean(output, "anti_aliased_lines_use_tex", style_.AntiAliasedLinesUseTex);
    WriteBoolean(output, "anti_aliased_fill", style_.AntiAliasedFill);
    WriteFloat(output, "curve_tessellation_tol", style_.CurveTessellationTol);
    WriteFloat(output, "circle_tessellation_max_error", style_.CircleTessellationMaxError);
    WriteFloat(output, "hover_stationary_delay", style_.HoverStationaryDelay);
    WriteFloat(output, "hover_delay_short", style_.HoverDelayShort);
    WriteFloat(output, "hover_delay_normal", style_.HoverDelayNormal);
    WriteInteger(output, "hover_flags_tooltip_mouse", style_.HoverFlagsForTooltipMouse);
    WriteInteger(output, "hover_flags_tooltip_nav", style_.HoverFlagsForTooltipNav);

    for (int index = 0; index < ImGuiCol_COUNT; ++index) {
        WriteVector4(output, "color." + std::to_string(index), style_.Colors[index]);
    }
}

} // namespace smf::core

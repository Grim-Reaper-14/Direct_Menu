#pragma once

#include <imgui.h>

#include <string>
#include <string_view>
#include <vector>

namespace smf::ui {

struct ThemeEntry {
    std::string id;
    std::string label;
    ImVec4 accent;
};

class ThemeManager {
public:
    ThemeManager();

    bool Apply(std::string_view id);
    [[nodiscard]] const std::vector<ThemeEntry>& Themes() const noexcept;
    [[nodiscard]] std::string_view Current() const noexcept;
    [[nodiscard]] ImVec4 Accent() const noexcept;

private:
    std::vector<ThemeEntry> themes_;
    std::string current_{"Reaper"};
    ImVec4 accent_{0.72F, 0.74F, 0.78F, 1.0F};
};

} // namespace smf::ui


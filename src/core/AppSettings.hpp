#pragma once

#include "core/ImGuiStyleSettings.hpp"

#include <string>

namespace smf::core {

struct AppSettings {
    std::string theme{"Reaper"};
    std::string font{"Segoe UI"};
    float fontScale{1.0F};
    std::string imagePath{};
    bool imageBackgroundEnabled{true};
    float imageBackgroundOpacity{0.42F};
    ImGuiStyleSettings imguiStyle{};
};

} // namespace smf::core

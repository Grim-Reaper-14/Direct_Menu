#pragma once

#include <string>

namespace smf::core {

struct AppSettings {
    std::string theme{"Midnight"};
    std::string font{"Segoe UI"};
    float fontScale{1.0F};
    std::string imagePath{};
};

} // namespace smf::core


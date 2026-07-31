#pragma once

#include <imgui.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace smf::logging {
class LoggerApi;
}

namespace smf::ui {

struct FontEntry {
    std::string id;
    std::string label;
    ImFont* font{nullptr};
};

class FontManager {
public:
    explicit FontManager(logging::LoggerApi& logger);

    void Initialize(const std::filesystem::path& customFontsDirectory);
    bool Select(std::string_view id);
    void SetScale(float scale);

    [[nodiscard]] const std::vector<FontEntry>& Fonts() const noexcept;
    [[nodiscard]] std::string_view Current() const noexcept;
    [[nodiscard]] float Scale() const noexcept;

private:
    void TryAdd(
        std::string id,
        std::string label,
        const std::filesystem::path& path,
        float sizePixels);

    logging::LoggerApi& logger_;
    std::vector<FontEntry> fonts_;
    std::string current_{"Default"};
    float scale_{1.0F};
};

} // namespace smf::ui


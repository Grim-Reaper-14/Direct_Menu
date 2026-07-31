#include "ui/FontManager.hpp"

#include "logging/Logger.hpp"

#include <algorithm>
#include <system_error>

namespace smf::ui {

FontManager::FontManager(logging::LoggerApi& logger)
    : logger_(logger) {
}

void FontManager::Initialize(const std::filesystem::path& customFontsDirectory) {
    fonts_.clear();

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig defaultConfig{};
    defaultConfig.SizePixels = 18.0F;
    ImFont* defaultFont = io.Fonts->AddFontDefault(&defaultConfig);
    fonts_.push_back({"Default", "Dear ImGui Default", defaultFont});

    TryAdd("Segoe UI", "Segoe UI", L"C:\\Windows\\Fonts\\segoeui.ttf", 18.0F);
    TryAdd("Consolas", "Consolas", L"C:\\Windows\\Fonts\\consola.ttf", 18.0F);
    TryAdd("Arial", "Arial", L"C:\\Windows\\Fonts\\arial.ttf", 18.0F);
    TryAdd("Tahoma", "Tahoma", L"C:\\Windows\\Fonts\\tahoma.ttf", 18.0F);

    std::error_code error;
    if (std::filesystem::exists(customFontsDirectory, error)) {
        for (const auto& entry : std::filesystem::directory_iterator(
                 customFontsDirectory,
                 error)) {
            if (error) {
                break;
            }

            const auto extension = entry.path().extension().wstring();
            if (!entry.is_regular_file(error) ||
                (extension != L".ttf" && extension != L".otf")) {
                continue;
            }

            const std::string stem = entry.path().stem().string();
            TryAdd("Custom:" + stem, stem + " (Custom)", entry.path(), 18.0F);
        }
    }

    Select(fonts_.size() > 1 ? fonts_[1].id : fonts_.front().id);
    logger_.Info("Loaded " + std::to_string(fonts_.size()) + " selectable font(s).");
}

bool FontManager::Select(const std::string_view id) {
    const auto found = std::ranges::find_if(
        fonts_,
        [id](const FontEntry& entry) {
            return entry.id == id;
        });
    if (found == fonts_.end() || found->font == nullptr) {
        return false;
    }

    ImGui::GetIO().FontDefault = found->font;
    current_ = found->id;
    return true;
}

void FontManager::SetScale(const float scale) {
    scale_ = std::clamp(scale, 0.75F, 1.50F);
    ImGui::GetIO().FontGlobalScale = scale_;
}

const std::vector<FontEntry>& FontManager::Fonts() const noexcept {
    return fonts_;
}

std::string_view FontManager::Current() const noexcept {
    return current_;
}

float FontManager::Scale() const noexcept {
    return scale_;
}

void FontManager::TryAdd(
    std::string id,
    std::string label,
    const std::filesystem::path& path,
    const float sizePixels) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return;
    }

    const std::string narrowPath = path.string();
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        narrowPath.c_str(),
        sizePixels);
    if (font == nullptr) {
        logger_.Warning("Could not load font: " + narrowPath);
        return;
    }

    fonts_.push_back({std::move(id), std::move(label), font});
}

} // namespace smf::ui


#include "ui/Menu.hpp"

#include "backend/D3D12Backend.hpp"
#include "config/ConfigManager.hpp"
#include "features/FeatureRegistry.hpp"
#include "filesystem/FileSystemManager.hpp"
#include "scripting/LuaManager.hpp"
#include "ui/FontManager.hpp"
#include "ui/ImageLoader.hpp"
#include "ui/NotificationCenter.hpp"
#include "ui/ThemeManager.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <format>
#include <string>
#include <variant>

namespace smf::ui {
namespace {

constexpr ImGuiWindowFlags RootWindowFlags =
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

bool ContainsCaseInsensitive(
    const std::string_view haystack,
    const std::string_view needle) {
    if (needle.empty()) {
        return true;
    }

    const auto found = std::search(
        haystack.begin(),
        haystack.end(),
        needle.begin(),
        needle.end(),
        [](const char left, const char right) {
            return std::tolower(static_cast<unsigned char>(left)) ==
                   std::tolower(static_cast<unsigned char>(right));
        });
    return found != haystack.end();
}

template <std::size_t Size>
void CopyToBuffer(std::array<char, Size>& destination, const std::string_view source) {
    destination.fill('\0');
    const std::size_t count = std::min(source.size(), Size - 1);
    std::copy_n(source.data(), count, destination.data());
}

ImU32 WithAlpha(const ImVec4 color, const float alpha) {
    return ImGui::ColorConvertFloat4ToU32(
        {color.x, color.y, color.z, alpha});
}

ImU32 ThemeImageTint(
    const ImVec4 accent,
    const float alpha,
    const float strength) {
    const float neutral = 1.0F - strength;
    return ImGui::ColorConvertFloat4ToU32({
        neutral + accent.x * strength,
        neutral + accent.y * strength,
        neutral + accent.z * strength,
        alpha
    });
}

} // namespace

Menu::Menu(
    const HWND window,
    backend::D3D12Backend& backend,
    config::ConfigManager& configs,
    features::FeatureRegistry& features,
    const filesystem::FileSystemManager& fileSystem,
    scripting::LuaManager& lua,
    FontManager& fonts,
    ImageLoader& brandBackground,
    ImageLoader& brandHeader,
    ImageLoader& images,
    NotificationCenter& notifications,
    ThemeManager& themes,
    core::AppSettings& settings,
    MenuCallbacks callbacks)
    : window_(window),
      backend_(backend),
      configs_(configs),
      features_(features),
      fileSystem_(fileSystem),
      lua_(lua),
      fonts_(fonts),
      brandBackground_(brandBackground),
      brandHeader_(brandHeader),
      images_(images),
      notifications_(notifications),
      themes_(themes),
      settings_(settings),
      callbacks_(std::move(callbacks)) {
    RefreshConfigurationNames();
}

void Menu::RenderWelcome() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    if (!ImGui::Begin("##welcome_root", nullptr, RootWindowFlags)) {
        ImGui::End();
        return;
    }

    DrawAppliedBackground();

    constexpr ImVec2 panelSize{430.0F, 560.0F};
    const ImVec2 available = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPos({
        std::max((available.x - panelSize.x) * 0.5F, 0.0F),
        std::max((available.y - panelSize.y) * 0.5F, 0.0F)
    });

    const ImVec4 accent = themes_.Accent();
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImVec4{
            accent.x * 0.035F,
            accent.y * 0.035F,
            accent.z * 0.035F,
            0.90F});
    ImGui::PushStyleColor(
        ImGuiCol_Border,
        ImVec4{accent.x, accent.y, accent.z, 0.62F});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0F);
    if (ImGui::BeginChild("##welcome_panel", panelSize, true)) {
        ImGui::Dummy({0.0F, 8.0F});
        DrawNeonHeader();

        ImGui::TextDisabled("Direct3D 12 / C++20 / Dear ImGui");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextWrapped(
            "Welcome. This is a modular standalone menu foundation with "
            "nested pages and a clean service architecture.");
        ImGui::Spacing();

        ImGui::BulletText("F5 hides and reopens the running menu");
        ImGui::BulletText("Themes recolor the menu and Reaper artwork live");
        ImGui::BulletText("Settings, fonts, images, and configs included");
        ImGui::BulletText("Feature pages are safe UI placeholders");

        ImGui::Dummy({0.0F, 20.0F});
        if (ImGui::Button("Open Menu", {-1.0F, 44.0F})) {
            welcomeComplete_ = true;
            notifications_.Push(
                "Menu ready. Press F5 to hide or reopen it.",
                NotificationKind::Success);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4{accent.x, accent.y, accent.z, 0.24F});
        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4{accent.x, accent.y, accent.z, 0.52F});
        if (ImGui::Button("Exit", {-1.0F, 36.0F}) && callbacks_.requestExit) {
            callbacks_.requestExit();
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::End();
}

void Menu::RenderMain() {
    if (navigation_.size() > 1 &&
        (ImGui::IsKeyPressed(ImGuiKey_Backspace) ||
         ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        Back();
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    if (!ImGui::Begin("##menu_root", nullptr, RootWindowFlags)) {
        ImGui::End();
        return;
    }

    DrawAppliedBackground();
    DrawNeonHeader();

    const char* hotkeyLabel = "F5  HIDE";
    const float hotkeyWidth = ImGui::CalcTextSize(hotkeyLabel).x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(
        std::max(
            ImGui::GetWindowContentRegionMax().x - hotkeyWidth,
            ImGui::GetCursorPosX()));
    ImGui::TextDisabled("%s", hotkeyLabel);

    ImGui::Separator();

    if (navigation_.size() > 1) {
        if (ImGui::Button("< Back", {82.0F, 30.0F})) {
            Back();
        }
        ImGui::SameLine();
    }
    const std::string_view pageTitle = PageTitle(CurrentPage());
    ImGui::TextColored(
        themes_.Accent(),
        "%.*s",
        static_cast<int>(pageTitle.size()),
        pageTitle.data());
    ImGui::Spacing();

    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + 16.0F;
    if (ImGui::BeginChild(
            "##page_content",
            {0.0F, -footerHeight},
            true,
            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        switch (CurrentPage()) {
        case MenuPage::Home:
            RenderHome();
            break;
        case MenuPage::Self:
            RenderSelf();
            break;
        case MenuPage::Weapons:
            RenderWeapons();
            break;
        case MenuPage::Teleport:
            RenderTeleport();
            break;
        case MenuPage::Unlocks:
            RenderUnlocks();
            break;
        case MenuPage::Vehicle:
            RenderVehicle();
            break;
        case MenuPage::VehicleSpawn:
            RenderVehicleSpawn();
            break;
        case MenuPage::Lsc:
            RenderLsc();
            break;
        case MenuPage::Settings:
            RenderSettings();
            break;
        case MenuPage::Lua:
            RenderLua();
            break;
        case MenuPage::Themes:
            RenderThemes();
            break;
        case MenuPage::Images:
            RenderImages();
            break;
        case MenuPage::Fonts:
            RenderFonts();
            break;
        case MenuPage::Configurations:
            RenderConfigurations();
            break;
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("Standalone framework  |  F5 visibility");
    ImGui::End();
}

bool Menu::WelcomeComplete() const noexcept {
    return welcomeComplete_;
}

void Menu::Navigate(const MenuPage page) {
    navigation_.push_back(page);
}

void Menu::Back() {
    if (navigation_.size() > 1) {
        navigation_.pop_back();
    }
}

MenuPage Menu::CurrentPage() const {
    return navigation_.back();
}

std::string_view Menu::PageTitle(const MenuPage page) {
    switch (page) {
    case MenuPage::Home:
        return "Main Menu";
    case MenuPage::Self:
        return "Self";
    case MenuPage::Weapons:
        return "Weapons";
    case MenuPage::Teleport:
        return "Teleport";
    case MenuPage::Unlocks:
        return "Unlocks";
    case MenuPage::Vehicle:
        return "Vehicle";
    case MenuPage::VehicleSpawn:
        return "Vehicle Spawn";
    case MenuPage::Lsc:
        return "LSC";
    case MenuPage::Settings:
        return "Settings";
    case MenuPage::Lua:
        return "Settings / Lua";
    case MenuPage::Themes:
        return "Settings / Themes";
    case MenuPage::Images:
        return "Settings / Image Loader";
    case MenuPage::Fonts:
        return "Settings / Font Selection";
    case MenuPage::Configurations:
        return "Settings / Save & Load";
    }
    return "Menu";
}

bool Menu::SubmenuButton(const std::string_view label, const MenuPage target) {
    const std::string buttonLabel = std::format("{}  >##{}", label, static_cast<int>(target));
    if (ImGui::Button(buttonLabel.c_str(), {-1.0F, 42.0F})) {
        Navigate(target);
        return true;
    }
    return false;
}

void Menu::DrawAppliedBackground() const {
    if (!settings_.imageBackgroundEnabled) {
        return;
    }

    const ImageLoader* source = nullptr;
    if (images_.HasImage()) {
        source = &images_;
    } else if (brandBackground_.HasImage()) {
        source = &brandBackground_;
    }

    if (source == nullptr) {
        return;
    }

    const auto& texture = source->Texture();
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    if (size.x <= 0.0F || size.y <= 0.0F ||
        texture.width == 0 || texture.height == 0) {
        return;
    }

    const float imageAspect =
        static_cast<float>(texture.width) / static_cast<float>(texture.height);
    const float windowAspect = size.x / size.y;
    ImVec2 uv0{0.0F, 0.0F};
    ImVec2 uv1{1.0F, 1.0F};
    if (imageAspect > windowAspect) {
        const float visibleWidth = windowAspect / imageAspect;
        uv0.x = (1.0F - visibleWidth) * 0.5F;
        uv1.x = 1.0F - uv0.x;
    } else {
        const float visibleHeight = imageAspect / windowAspect;
        uv0.y = (1.0F - visibleHeight) * 0.5F;
        uv1.y = 1.0F - uv0.y;
    }

    const ImVec4 accent = themes_.Accent();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddImage(
        static_cast<ImTextureID>(texture.gpuDescriptor.ptr),
        position,
        {position.x + size.x, position.y + size.y},
        uv0,
        uv1,
        ThemeImageTint(
            accent,
            settings_.imageBackgroundOpacity,
            0.42F));

    drawList->AddRectFilled(
        position,
        {position.x + size.x, position.y + size.y},
        ImGui::ColorConvertFloat4ToU32({
            accent.x * 0.07F,
            accent.y * 0.07F,
            accent.z * 0.07F,
            0.30F}));
    drawList->AddRectFilled(
        position,
        {position.x + size.x, position.y + size.y},
        WithAlpha(accent, 0.035F));
}

void Menu::DrawNeonHeader() const {
    const ImVec4 accent = themes_.Accent();

    if (brandHeader_.HasImage()) {
        const auto& texture = brandHeader_.Texture();
        if (texture.width > 0 && texture.height > 0) {
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float aspect =
                static_cast<float>(texture.height) /
                static_cast<float>(texture.width);

            float width = availableWidth;
            float height = width * aspect;
            constexpr float maximumHeight = 126.0F;
            if (height > maximumHeight) {
                height = maximumHeight;
                width = height / aspect;
            }

            const float offsetX = std::max((availableWidth - width) * 0.5F, 0.0F);
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const ImVec2 topLeft{cursor.x + offsetX, cursor.y};
            const ImVec2 bottomRight{topLeft.x + width, topLeft.y + height};
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddImage(
                static_cast<ImTextureID>(texture.gpuDescriptor.ptr),
                topLeft,
                bottomRight,
                {0.0F, 0.0F},
                {1.0F, 1.0F},
                ThemeImageTint(accent, 1.0F, 0.50F));
            drawList->AddRectFilled(
                topLeft,
                bottomRight,
                WithAlpha(accent, 0.055F),
                7.0F);
            drawList->AddRect(
                topLeft,
                bottomRight,
                WithAlpha(accent, 0.82F),
                7.0F,
                0,
                1.5F);

            ImGui::Dummy({availableWidth, height + 8.0F});
            return;
        }
    }

    constexpr std::string_view title{"DIRECT // MENU"};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize() * 1.34F;
    const ImVec2 position = ImGui::GetCursorScreenPos();

    for (const ImVec2 offset : {
             ImVec2{-2.0F, 0.0F},
             ImVec2{2.0F, 0.0F},
             ImVec2{0.0F, -2.0F},
             ImVec2{0.0F, 2.0F}}) {
        drawList->AddText(
            font,
            size,
            {position.x + offset.x, position.y + offset.y},
            WithAlpha(accent, 0.20F),
            title.data(),
            title.data() + title.size());
    }
    drawList->AddText(
        font,
        size,
        position,
        WithAlpha(accent, 1.0F),
        title.data(),
        title.data() + title.size());

    ImVec4 highlight = accent;
    highlight.x = std::min(highlight.x + 0.30F, 1.0F);
    highlight.y = std::min(highlight.y + 0.30F, 1.0F);
    highlight.z = std::min(highlight.z + 0.30F, 1.0F);
    drawList->AddText(
        font,
        size,
        {position.x, position.y - 1.0F},
        WithAlpha(highlight, 0.88F),
        title.data(),
        title.data() + title.size());

    const ImVec2 textSize = font->CalcTextSizeA(
        size,
        FLT_MAX,
        0.0F,
        title.data(),
        title.data() + title.size());
    const float lineY = position.y + textSize.y + 3.0F;
    drawList->AddLine(
        {position.x, lineY},
        {position.x + textSize.x, lineY},
        WithAlpha(accent, 0.75F),
        1.5F);
    ImGui::Dummy({textSize.x, textSize.y + 7.0F});
}

void Menu::RenderInfoCard(const std::string_view text) const {
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        {themes_.Accent().x, themes_.Accent().y, themes_.Accent().z, 0.10F});
    ImGui::PushStyleColor(
        ImGuiCol_Border,
        {themes_.Accent().x, themes_.Accent().y, themes_.Accent().z, 0.40F});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0F);
    if (ImGui::BeginChild(
            "##info_card",
            {0.0F, 62.0F},
            true,
            ImGuiWindowFlags_NoScrollbar)) {
        ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

void Menu::RenderFeatureCategory(const std::string_view category) {
    const auto categoryFeatures = features_.InCategory(category);
    if (categoryFeatures.empty()) {
        ImGui::TextDisabled("No features are registered in this category.");
        return;
    }

    for (features::Feature* feature : categoryFeatures) {
        RenderFeature(*feature);
    }
}

void Menu::RenderFeature(features::Feature& feature) {
    ImGui::PushID(feature.id.c_str());

    switch (feature.kind) {
    case features::FeatureKind::Toggle: {
        bool* value = std::get_if<bool>(&feature.value);
        if (value != nullptr) {
            ImGui::Checkbox(feature.label.c_str(), value);
        }
        break;
    }
    case features::FeatureKind::Integer: {
        int* value = std::get_if<int>(&feature.value);
        if (value != nullptr) {
            ImGui::SliderInt(
                feature.label.c_str(),
                value,
                static_cast<int>(feature.minimum),
                static_cast<int>(feature.maximum));
        }
        break;
    }
    case features::FeatureKind::Float: {
        float* value = std::get_if<float>(&feature.value);
        if (value != nullptr) {
            ImGui::SliderFloat(
                feature.label.c_str(),
                value,
                feature.minimum,
                feature.maximum,
                "%.2f");
        }
        break;
    }
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::TextWrapped("%s", feature.description.c_str());
        if (feature.placeholder) {
            ImGui::Spacing();
            ImGui::TextColored(
                {0.95F, 0.70F, 0.24F, 1.0F},
                "UI-only placeholder: no provider is connected.");
        }
        ImGui::EndTooltip();
    }

    ImGui::Separator();
    ImGui::PopID();
}

void Menu::RenderHome() {
    ImGui::InputTextWithHint(
        "##feature_search",
        "Search registered features...",
        searchText_.data(),
        searchText_.size());

    if (searchText_.front() != '\0') {
        const std::string query{searchText_.data()};
        bool anyFound = false;
        constexpr std::array categories{
            "Self",
            "Weapons",
            "Unlocks",
            "Vehicle",
            "LSC"
        };

        for (const std::string_view category : categories) {
            for (features::Feature* feature : features_.InCategory(category)) {
                if (!ContainsCaseInsensitive(feature->label, query) &&
                    !ContainsCaseInsensitive(feature->description, query)) {
                    continue;
                }

                if (!anyFound) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Search results");
                }
                anyFound = true;
                ImGui::TextColored(themes_.Accent(), "%s", feature->category.c_str());
                RenderFeature(*feature);
            }
        }

        if (!anyFound) {
            ImGui::Spacing();
            ImGui::TextDisabled("No matching registered features.");
        }
        return;
    }

    ImGui::Spacing();
    SubmenuButton("Self", MenuPage::Self);
    SubmenuButton("Weapons", MenuPage::Weapons);
    SubmenuButton("Teleport", MenuPage::Teleport);
    SubmenuButton("Unlocks", MenuPage::Unlocks);
    SubmenuButton("Vehicle", MenuPage::Vehicle);
    ImGui::Spacing();
    SubmenuButton("Settings", MenuPage::Settings);
}

void Menu::RenderSelf() {
    RenderInfoCard(
        "Self options are registered through the shared feature API. "
        "They do not control another program in this bare-bones build.");
    RenderFeatureCategory("Self");
}

void Menu::RenderWeapons() {
    RenderInfoCard(
        "Weapons contains UI placeholders only. Native providers can be "
        "connected later without changing the menu or configuration API.");
    RenderFeatureCategory("Weapons");
}

void Menu::RenderTeleport() {
    RenderInfoCard(
        "Enter coordinates to test the UI request flow. No game or map "
        "provider is connected.");

    ImGui::InputFloat3("Position", teleportPosition_.data(), "%.2f");
    if (ImGui::Button("Submit Teleport Request", {-1.0F, 40.0F})) {
        notifications_.Push(
            "Teleport request captured; no provider is connected.",
            NotificationKind::Info);
    }
}

void Menu::RenderUnlocks() {
    RenderInfoCard(
        "Unlock entries are visual state only. This framework does not alter "
        "accounts, protected content, or online services.");
    RenderFeatureCategory("Unlocks");
}

void Menu::RenderVehicle() {
    RenderInfoCard(
        "Vehicle options use the same central feature registry as every "
        "other native page.");
    RenderFeatureCategory("Vehicle");
    ImGui::Spacing();
    SubmenuButton("Vehicle Spawn", MenuPage::VehicleSpawn);
}

void Menu::RenderVehicleSpawn() {
    RenderInfoCard(
        "The spawn form demonstrates validation and notifications. It does "
        "not communicate with a game process.");

    ImGui::InputText(
        "Model",
        vehicleModel_.data(),
        vehicleModel_.size());
    if (ImGui::Button("Submit Spawn Request", {-1.0F, 40.0F})) {
        if (vehicleModel_.front() == '\0') {
            notifications_.Push(
                "Enter a model name first.",
                NotificationKind::Warning);
        } else {
            notifications_.Push(
                std::format(
                    "Spawn request for '{}' captured; no provider is connected.",
                    vehicleModel_.data()),
                NotificationKind::Info);
        }
    }

    ImGui::Spacing();
    SubmenuButton("LSC Customization", MenuPage::Lsc);
}

void Menu::RenderLsc() {
    RenderInfoCard(
        "LSC values are persisted as typed feature entries and are ready for "
        "a legitimate native provider later.");
    RenderFeatureCategory("LSC");

    if (ImGui::Button("Submit LSC Request", {-1.0F, 40.0F})) {
        notifications_.Push(
            "LSC request captured; no provider is connected.",
            NotificationKind::Info);
    }
}

void Menu::RenderSettings() {
    SubmenuButton("Lua", MenuPage::Lua);
    SubmenuButton("Themes", MenuPage::Themes);
    SubmenuButton("Image Loader", MenuPage::Images);
    SubmenuButton("Font Selection", MenuPage::Fonts);
    SubmenuButton("Save / Load Settings", MenuPage::Configurations);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Hide Menu (F5)", {-1.0F, 40.0F}) &&
        callbacks_.toggleVisibility) {
        callbacks_.toggleVisibility();
    }

    const ImVec4 accent = themes_.Accent();
    ImGui::PushStyleColor(
        ImGuiCol_Button,
        ImVec4{accent.x, accent.y, accent.z, 0.26F});
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImVec4{accent.x, accent.y, accent.z, 0.56F});
    if (ImGui::Button("Exit Menu", {-1.0F, 40.0F}) && callbacks_.requestExit) {
        callbacks_.requestExit();
    }
    ImGui::PopStyleColor(2);
}

void Menu::RenderLua() {
    RenderInfoCard(lua_.StatusText());

    if (ImGui::Button("Refresh Script Folder", {-1.0F, 38.0F})) {
        lua_.Refresh();
        notifications_.Push("Lua script list refreshed.", NotificationKind::Success);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Detected .lua files");
    if (lua_.Scripts().empty()) {
        ImGui::TextWrapped(
            "No scripts found. Files can be placed in the LuaScripts runtime "
            "folder, but execution stays disabled until bindings are added.");
    } else {
        for (const auto& script : lua_.Scripts()) {
            ImGui::BulletText("%s", script.name.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::BeginDisabled();
    ImGui::Button("Load Selected Script (Runtime Deferred)", {-1.0F, 38.0F});
    ImGui::EndDisabled();
}

void Menu::RenderThemes() {
    RenderInfoCard(
        "Theme changes recolor controls, borders, headers, and Reaper artwork immediately.");

    for (const ThemeEntry& theme : themes_.Themes()) {
        ImGui::PushID(theme.id.c_str());
        ImGui::ColorButton(
            "##accent",
            theme.accent,
            ImGuiColorEditFlags_NoTooltip,
            {24.0F, 24.0F});
        ImGui::SameLine();

        const bool selected = themes_.Current() == theme.id;
        if (ImGui::Selectable(theme.label.c_str(), selected)) {
            themes_.Apply(theme.id);
            settings_.theme = theme.id;
            notifications_.Push(
                "Theme changed to " + theme.label + '.',
                NotificationKind::Success);
        }
        ImGui::PopID();
    }
}

void Menu::RenderImages() {
    RenderInfoCard(
        "Load PNG, JPG, BMP, or TIFF images through Windows Imaging "
        "Component. A custom image overrides the built-in Reaper background; "
        "clearing it restores the built-in artwork.");

    ImGui::InputText(
        "Image Path",
        imagePath_.data(),
        imagePath_.size());

    if (ImGui::Button("Browse...", {110.0F, 36.0F})) {
        const auto selected = fileSystem_.OpenImageDialog(window_);
        if (selected.has_value()) {
            CopyToBuffer(
                imagePath_,
                filesystem::FileSystemManager::ToUtf8(selected->wstring()));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Image", {-1.0F, 36.0F})) {
        if (imagePath_.front() == '\0') {
            notifications_.Push("Choose an image first.", NotificationKind::Warning);
        } else {
            const std::filesystem::path path{
                filesystem::FileSystemManager::ToWide(imagePath_.data())};
            std::string error;
            if (images_.Load(path, backend_, error)) {
                settings_.imagePath = imagePath_.data();
                notifications_.Push("Image loaded.", NotificationKind::Success);
            } else {
                notifications_.Push(error, NotificationKind::Error, 5.0);
            }
        }
    }

    if (images_.HasImage()) {
        ImGui::Spacing();
        ImGui::Checkbox(
            "Apply as Menu Background",
            &settings_.imageBackgroundEnabled);
        ImGui::SliderFloat(
            "Background Brightness",
            &settings_.imageBackgroundOpacity,
            0.05F,
            1.0F,
            "%.2f");

        const auto& texture = images_.Texture();
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float maximumWidth = std::min(availableWidth, 360.0F);
        const float aspect =
            static_cast<float>(texture.height) / static_cast<float>(texture.width);
        ImVec2 previewSize{maximumWidth, maximumWidth * aspect};
        if (previewSize.y > 240.0F) {
            previewSize.x *= 240.0F / previewSize.y;
            previewSize.y = 240.0F;
        }

        ImGui::Text(
            "%u x %u  |  %s",
            texture.width,
            texture.height,
            images_.Path().filename().string().c_str());
        ImGui::Image(
            static_cast<ImTextureID>(texture.gpuDescriptor.ptr),
            previewSize);

        if (ImGui::Button("Clear Custom Image", {-1.0F, 36.0F})) {
            images_.Clear(backend_);
            imagePath_.fill('\0');
            settings_.imagePath.clear();
            notifications_.Push(
                "Custom image cleared; built-in Reaper background restored.",
                NotificationKind::Info);
        }
    } else if (brandBackground_.HasImage()) {
        ImGui::Spacing();
        ImGui::Checkbox(
            "Apply Built-in Reaper Background",
            &settings_.imageBackgroundEnabled);
        ImGui::SliderFloat(
            "Background Brightness",
            &settings_.imageBackgroundOpacity,
            0.05F,
            1.0F,
            "%.2f");
        ImGui::TextDisabled("Built-in Reaper background is active.");
    }
}

void Menu::RenderFonts() {
    RenderInfoCard(
        "All available fonts are preloaded into one atlas. Selecting a font "
        "switches it instantly without rebuilding D3D12 resources.");

    for (const FontEntry& font : fonts_.Fonts()) {
        const bool selected = fonts_.Current() == font.id;
        if (ImGui::RadioButton(font.label.c_str(), selected)) {
            fonts_.Select(font.id);
            settings_.font = font.id;
            notifications_.Push(
                "Font changed to " + font.label + '.',
                NotificationKind::Success);
        }
    }

    ImGui::Spacing();
    float scale = fonts_.Scale();
    if (ImGui::SliderFloat("Live Font Scale", &scale, 0.75F, 1.50F, "%.2f")) {
        fonts_.SetScale(scale);
        settings_.fontScale = fonts_.Scale();
    }

    ImGui::TextDisabled(
        "Place additional .ttf or .otf files in the Fonts runtime folder "
        "before launching the program.");
}

void Menu::RenderConfigurations() {
    RenderInfoCard(
        "Configurations save the selected theme, font, image path, and every "
        "registered feature value.");

    ImGui::InputText(
        "Config Name",
        configurationName_.data(),
        configurationName_.size());

    if (ImGui::BeginCombo("Available", configurationName_.data())) {
        for (const std::string& name : configurationNames_) {
            const bool selected = name == configurationName_.data();
            if (ImGui::Selectable(name.c_str(), selected)) {
                CopyToBuffer(configurationName_, name);
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Save Settings", {-1.0F, 40.0F})) {
        settings_.theme = std::string{themes_.Current()};
        settings_.font = std::string{fonts_.Current()};
        settings_.fontScale = fonts_.Scale();
        if (images_.HasImage()) {
            settings_.imagePath =
                filesystem::FileSystemManager::ToUtf8(images_.Path().wstring());
        }

        std::string error;
        if (configs_.Save(
                configurationName_.data(),
                settings_,
                features_,
                error)) {
            RefreshConfigurationNames();
            notifications_.Push("Settings saved.", NotificationKind::Success);
        } else {
            notifications_.Push(error, NotificationKind::Error, 5.0);
        }
    }

    if (ImGui::Button("Load Settings", {-1.0F, 40.0F})) {
        std::string error;
        if (configs_.Load(
                configurationName_.data(),
                settings_,
                features_,
                error)) {
            ApplyLoadedSettings();
            notifications_.Push("Settings loaded.", NotificationKind::Success);
        } else {
            notifications_.Push(error, NotificationKind::Error, 5.0);
        }
    }

    if (ImGui::Button("Reset Feature Values", {-1.0F, 36.0F})) {
        features_.ResetToDefaults();
        notifications_.Push(
            "Registered features reset to defaults.",
            NotificationKind::Info);
    }
}

void Menu::ApplyLoadedSettings() {
    if (!themes_.Apply(settings_.theme)) {
        settings_.theme = "Midnight";
        themes_.Apply(settings_.theme);
    }

    if (!fonts_.Select(settings_.font)) {
        settings_.font = "Default";
        fonts_.Select(settings_.font);
    }
    fonts_.SetScale(settings_.fontScale);

    if (!settings_.imagePath.empty()) {
        CopyToBuffer(imagePath_, settings_.imagePath);
        const std::filesystem::path path{
            filesystem::FileSystemManager::ToWide(settings_.imagePath)};
        std::string imageError;
        if (!images_.Load(path, backend_, imageError)) {
            notifications_.Push(
                "Settings loaded, but the saved image could not be loaded. "
                "The built-in Reaper background will be used instead.",
                NotificationKind::Warning,
                5.0);
        }
    } else if (images_.HasImage()) {
        images_.Clear(backend_);
        imagePath_.fill('\0');
    }
}

void Menu::RefreshConfigurationNames() {
    configurationNames_ = configs_.Available();
}

} // namespace smf::ui

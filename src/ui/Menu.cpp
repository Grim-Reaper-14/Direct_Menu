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
    ImageLoader& brandIcon,
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
      brandIcon_(brandIcon),
      images_(images),
      notifications_(notifications),
      themes_(themes),
      settings_(settings),
      callbacks_(std::move(callbacks)) {
    RefreshConfigurationNames();

    const std::string startupConfiguration =
        configs_.StartupConfiguration();
    if (std::ranges::find(configurationNames_, startupConfiguration) !=
        configurationNames_.end()) {
        CopyToBuffer(configurationName_, startupConfiguration);
        std::string error;
        if (configs_.Load(
                startupConfiguration,
                settings_,
                features_,
                error)) {
            ApplyLoadedSettings();
        }
    }
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
    const ImVec2 headerTopLeft = ImGui::GetCursorScreenPos();
    DrawNeonHeader();

    const char* hotkeyLabel = "F5  HIDE";
    const ImVec2 hotkeySize = ImGui::CalcTextSize(hotkeyLabel);
    const ImVec2 windowPosition = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 hotkeyPosition{
        windowPosition.x + windowSize.x - hotkeySize.x - 12.0F,
        headerTopLeft.y + 10.0F};
    ImDrawList* headerDrawList = ImGui::GetWindowDrawList();
    headerDrawList->AddRectFilled(
        {hotkeyPosition.x - 6.0F, hotkeyPosition.y - 4.0F},
        {hotkeyPosition.x + hotkeySize.x + 6.0F,
         hotkeyPosition.y + hotkeySize.y + 4.0F},
        IM_COL32(0, 0, 0, 150),
        5.0F);
    headerDrawList->AddText(
        hotkeyPosition,
        ImGui::GetColorU32(ImGuiCol_TextDisabled),
        hotkeyLabel);

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

    constexpr float footerHeight = 76.0F;
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
        case MenuPage::Network:
            RenderNetwork();
            break;
        case MenuPage::OnlineSessions:
            RenderOnlineSessions();
            break;
        case MenuPage::Misc:
            RenderMisc();
            break;
        case MenuPage::Settings:
            RenderSettings();
            break;
        case MenuPage::Lua:
            RenderLua();
            break;
        case MenuPage::StyleEditor:
            RenderStyleEditor();
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

    ImGui::Spacing();
    DrawBottomHeader();
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
    case MenuPage::Network:
        return "Network";
    case MenuPage::OnlineSessions:
        return "Online Sessions";
    case MenuPage::Misc:
        return "Miscellaneous";
    case MenuPage::Settings:
        return "Settings";
    case MenuPage::Lua:
        return "Settings / Lua";
    case MenuPage::StyleEditor:
        return "Settings / Style Editor";
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

std::string_view Menu::PageDescription(const MenuPage page) {
    switch (page) {
    case MenuPage::Home:
        return "Choose a menu category or search the registered features from one place.";
    case MenuPage::Self:
        return "Personal state and movement controls registered through the shared feature system.";
    case MenuPage::Weapons:
        return "Weapon-related options and adjustable values exposed by the feature registry.";
    case MenuPage::Teleport:
        return "Enter position coordinates and submit teleport requests through the menu interface.";
    case MenuPage::Unlocks:
        return "Local preview and unlock-related settings grouped into one dedicated page.";
    case MenuPage::Vehicle:
        return "Vehicle state, repair, speed, spawning, and customization pages are accessed here.";
    case MenuPage::VehicleSpawn:
        return "Enter a vehicle model name, validate it, and submit the spawn request from this page.";
    case MenuPage::Lsc:
        return "Adjust the stored vehicle customization values used by the LSC section.";
    case MenuPage::Network:
        return "Review local network-interface preferences and open the nested Online Sessions page.";
    case MenuPage::OnlineSessions:
        return "Manage saved session-list preferences for a future legitimate provider.";
    case MenuPage::Misc:
        return "Adjust saved quality-of-life preferences for the Direct Menu interface.";
    case MenuPage::Settings:
        return "Manage Lua, themes, backgrounds, fonts, configurations, visibility, and application controls.";
    case MenuPage::Lua:
        return "Refresh and manage Lua scripts and review the current scripting runtime status.";
    case MenuPage::StyleEditor:
        return "Inspect and edit the active Dear ImGui style, with controls to restore the selected Direct Menu theme.";
    case MenuPage::Themes:
        return "Change the active accent theme; controls, headers, borders, and Reaper artwork update live.";
    case MenuPage::Images:
        return "Load a custom menu background or restore the built-in theme-tinted Reaper artwork.";
    case MenuPage::Fonts:
        return "Choose a loaded font and adjust its live scale across the menu interface.";
    case MenuPage::Configurations:
        return "Save, load, and restore menu settings and registered feature values.";
    }
    return "Direct Menu navigation and controls.";
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
            const float bannerWidth = ImGui::GetWindowSize().x;
            const float aspect =
                static_cast<float>(texture.height) /
                static_cast<float>(texture.width);

            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const ImVec2 topLeft{ImGui::GetWindowPos().x, cursor.y};
            const float bannerHeight = bannerWidth * aspect;
            const ImVec2 bottomRight{
                topLeft.x + bannerWidth,
                topLeft.y + bannerHeight};
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

            ImGui::Dummy({availableWidth, bannerHeight + 8.0F});
            return;
        }
    }

    constexpr std::string_view title{"DIRECT MENU"};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize() * 1.34F;
    const ImVec2 position = ImGui::GetCursorScreenPos();

    const ImVec2 textSize = font->CalcTextSizeA(
        size,
        FLT_MAX,
        0.0F,
        title.data(),
        title.data() + title.size());
    const bool showIcons = brandIcon_.HasImage();
    const float iconSize =
        showIcons ? std::max(textSize.y + 8.0F, 30.0F) : 0.0F;
    const float iconSpacing = showIcons ? 7.0F : 0.0F;
    const float totalWidth =
        textSize.x + (iconSize + iconSpacing) * 2.0F;
    const float totalHeight = std::max(textSize.y, iconSize);
    const ImVec2 textPosition{
        position.x + iconSize + iconSpacing,
        position.y + (totalHeight - textSize.y) * 0.5F};

    if (showIcons) {
        const backend::TextureResource& texture = brandIcon_.Texture();
        const ImTextureID textureId =
            static_cast<ImTextureID>(texture.gpuDescriptor.ptr);
        drawList->AddImage(
            textureId,
            position,
            {position.x + iconSize, position.y + iconSize});
        const ImVec2 rightIconPosition{
            textPosition.x + textSize.x + iconSpacing,
            position.y};
        drawList->AddImage(
            textureId,
            rightIconPosition,
            {rightIconPosition.x + iconSize,
             rightIconPosition.y + iconSize});
    }

    for (const ImVec2 offset : {
             ImVec2{-2.0F, 0.0F},
             ImVec2{2.0F, 0.0F},
             ImVec2{0.0F, -2.0F},
             ImVec2{0.0F, 2.0F}}) {
        drawList->AddText(
            font,
            size,
            {textPosition.x + offset.x, textPosition.y + offset.y},
            WithAlpha(accent, 0.20F),
            title.data(),
            title.data() + title.size());
    }
    drawList->AddText(
        font,
        size,
        textPosition,
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
        {textPosition.x, textPosition.y - 1.0F},
        WithAlpha(highlight, 0.88F),
        title.data(),
        title.data() + title.size());

    const float lineY = position.y + totalHeight + 3.0F;
    drawList->AddLine(
        {position.x, lineY},
        {position.x + totalWidth, lineY},
        WithAlpha(accent, 0.75F),
        1.5F);
    ImGui::Dummy({totalWidth, totalHeight + 7.0F});
}

void Menu::DrawBottomHeader() const {
    const MenuPage page = CurrentPage();
    const std::string_view title = PageTitle(page);
    const std::string_view description = PageDescription(page);
    const ImVec4 accent = themes_.Accent();

    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImVec4{
            accent.x * 0.045F,
            accent.y * 0.045F,
            accent.z * 0.045F,
            0.88F});
    ImGui::PushStyleColor(
        ImGuiCol_Border,
        ImVec4{accent.x, accent.y, accent.z, 0.58F});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{10.0F, 7.0F});

    if (ImGui::BeginChild(
            "##bottom_header",
            {0.0F, 62.0F},
            true,
            ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::TextColored(
            accent,
            "%.*s",
            static_cast<int>(title.size()),
            title.data());
        ImGui::SameLine();
        ImGui::TextDisabled("|  F5 HIDE");
        ImGui::TextWrapped(
            "%.*s",
            static_cast<int>(description.size()),
            description.data());
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
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
            "LSC",
            "Network",
            "Online Sessions",
            "Misc"
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
    SubmenuButton("Network", MenuPage::Network);
    SubmenuButton("Miscellaneous", MenuPage::Misc);
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

void Menu::RenderNetwork() {
    RenderInfoCard(
        "Network options are local interface preferences and diagnostics "
        "placeholders. This standalone build does not contact a server.");

    SubmenuButton("Online Sessions", MenuPage::OnlineSessions);
    ImGui::Spacing();
    ImGui::SeparatorText("Network Diagnostics");
    RenderFeatureCategory("Network");

    if (ImGui::Button("Refresh Network Status", {-1.0F, 40.0F})) {
        notifications_.Push(
            "Network status refresh simulated; no provider is connected.",
            NotificationKind::Info);
    }
}

void Menu::RenderOnlineSessions() {
    RenderInfoCard(
        "Online Sessions demonstrates filters and session-list controls. "
        "It does not enumerate, join, or modify real online sessions.");
    RenderFeatureCategory("Online Sessions");

    const float buttonWidth =
        (ImGui::GetContentRegionAvail().x -
         ImGui::GetStyle().ItemSpacing.x) * 0.5F;
    if (ImGui::Button("Refresh Sessions", {buttonWidth, 40.0F})) {
        notifications_.Push(
            "Session refresh simulated; no provider is connected.",
            NotificationKind::Info);
    }
    ImGui::SameLine();
    if (ImGui::Button("Quick Join", {-1.0F, 40.0F})) {
        notifications_.Push(
            "Quick Join is a UI placeholder in this standalone build.",
            NotificationKind::Warning);
    }
}

void Menu::RenderMisc() {
    RenderInfoCard(
        "Miscellaneous contains saved quality-of-life preferences for the "
        "menu interface.");
    RenderFeatureCategory("Misc");

    if (ImGui::Button("Test Notification", {-1.0F, 40.0F})) {
        notifications_.Push(
            "Miscellaneous notification test successful.",
            NotificationKind::Success);
    }
}

void Menu::RenderSettings() {
    SubmenuButton("Lua", MenuPage::Lua);
    SubmenuButton("Style Editor", MenuPage::StyleEditor);
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

    ImGui::TextDisabled("Script interface");
    if (lua_.HasMenuContent()) {
        ImGui::PushID("LuaScriptInterface");
        lua_.DrawMenu();
        ImGui::PopID();
    } else {
        ImGui::TextWrapped(
            "Loaded scripts can add controls here with direct.ui or "
            "event.on(\"menu\", function() ... end). Scripts may also use "
            "the immediate-mode ImGui/imgui table inside menu and draw callbacks.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto& scriptsManager = lua_.ScriptsManager();
    const std::string scriptsDirectory =
        filesystem::FileSystemManager::ToUtf8(
            scriptsManager.Directory().wstring());

    ImGui::TextDisabled("Lua script folder");
    ImGui::TextWrapped("%s", scriptsDirectory.c_str());
    if (ImGui::Button("Copy Folder Path", {150.0F, 36.0F})) {
        ImGui::SetClipboardText(scriptsDirectory.c_str());
        notifications_.Push(
            "Lua script folder copied to the clipboard.",
            NotificationKind::Success);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh Scripts", {150.0F, 36.0F})) {
        lua_.Refresh();
        notifications_.Push(
            std::format(
                "Lua script list refreshed: {} file(s) found.",
                scriptsManager.Scripts().size()),
            NotificationKind::Success);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload All", {-1.0F, 36.0F})) {
        std::vector<std::string> names;
        names.reserve(scriptsManager.Scripts().size());
        for (const auto& script : scriptsManager.Scripts()) {
            if (script.enabled) {
                names.push_back(script.name);
            }
        }

        std::size_t loadedCount = 0;
        for (const std::string& name : names) {
            if (scriptsManager.Reload(name)) {
                ++loadedCount;
            }
        }

        const std::size_t failedCount = names.size() - loadedCount;
        if (failedCount == 0) {
            notifications_.Push(
                std::format("Reloaded {} Lua script(s).", loadedCount),
                NotificationKind::Success);
        } else {
            notifications_.Push(
                std::format(
                    "Reloaded {} Lua script(s); {} failed. See each script's error below.",
                    loadedCount,
                    failedCount),
                NotificationKind::Error,
                6.0);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Scripts");
    if (scriptsManager.Scripts().empty()) {
        ImGui::TextWrapped(
            "No .lua files were found. Place scripts in the folder shown above, "
            "then select Refresh Scripts.");
    } else {
        for (const auto& script : scriptsManager.Scripts()) {
            ImGui::PushID(script.name.c_str());

            const ImVec4 statusColor = script.loaded
                ? ImVec4{0.24F, 0.78F, 0.48F, 1.0F}
                : (script.lastError.empty()
                    ? ImVec4{0.70F, 0.72F, 0.76F, 1.0F}
                    : ImVec4{0.92F, 0.30F, 0.34F, 1.0F});
            ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
            ImGui::TextUnformatted(
                script.loaded ? "LOADED" :
                (script.lastError.empty() ? "UNLOADED" : "ERROR"));
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextUnformatted(script.name.c_str());

            bool enabled = script.enabled;
            if (ImGui::Checkbox("Enabled", &enabled)) {
                const std::string name = script.name;
                scripting::ScriptRecord* mutableScript =
                    scriptsManager.Find(name);
                if (mutableScript != nullptr) {
                    if (!enabled && mutableScript->loaded) {
                        (void)scriptsManager.Unload(name);
                    }
                    mutableScript->enabled = enabled;
                    if (enabled && mutableScript->autoLoad &&
                        !mutableScript->loaded) {
                        (void)scriptsManager.Load(name);
                    }
                    notifications_.Push(
                        std::format(
                            "{} Lua script: {}",
                            enabled ? "Enabled" : "Disabled",
                            name),
                        NotificationKind::Success);
                }
            }
            ImGui::SameLine();
            bool autoLoad = script.autoLoad;
            if (ImGui::Checkbox("AutoLoad", &autoLoad)) {
                const std::string name = script.name;
                scripting::ScriptRecord* mutableScript =
                    scriptsManager.Find(name);
                if (mutableScript != nullptr) {
                    mutableScript->autoLoad = autoLoad;
                    if (autoLoad && mutableScript->enabled &&
                        !mutableScript->loaded) {
                        (void)scriptsManager.Load(name);
                    }
                    notifications_.Push(
                        std::format(
                            "AutoLoad {} for {} (this session).",
                            autoLoad ? "enabled" : "disabled",
                            name),
                        NotificationKind::Info);
                }
            }

            ImGui::BeginDisabled(script.loaded || !script.enabled);
            if (ImGui::Button("Load", {92.0F, 34.0F})) {
                const std::string name = script.name;
                if (scriptsManager.Load(name)) {
                    notifications_.Push(
                        "Loaded Lua script: " + name,
                        NotificationKind::Success);
                } else {
                    const auto* failed = scriptsManager.Find(name);
                    const std::string detail =
                        failed != nullptr && !failed->lastError.empty()
                            ? failed->lastError
                            : "The script is disabled or could not be loaded.";
                    notifications_.Push(
                        "Failed to load " + name + ": " + detail,
                        NotificationKind::Error,
                        6.0);
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(!script.enabled);
            if (ImGui::Button("Reload", {92.0F, 34.0F})) {
                const std::string name = script.name;
                if (scriptsManager.Reload(name)) {
                    notifications_.Push(
                        "Reloaded Lua script: " + name,
                        NotificationKind::Success);
                } else {
                    const auto* failed = scriptsManager.Find(name);
                    const std::string detail =
                        failed != nullptr && !failed->lastError.empty()
                            ? failed->lastError
                            : "The script could not be reloaded.";
                    notifications_.Push(
                        "Failed to reload " + name + ": " + detail,
                        NotificationKind::Error,
                        6.0);
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(!script.loaded);
            if (ImGui::Button("Unload", {-1.0F, 34.0F})) {
                const std::string name = script.name;
                if (scriptsManager.Unload(name)) {
                    notifications_.Push(
                        "Unloaded Lua script: " + name,
                        NotificationKind::Success);
                } else {
                    notifications_.Push(
                        "Could not unload Lua script: " + name,
                        NotificationKind::Warning);
                }
            }
            ImGui::EndDisabled();

            ImGui::TextDisabled(
                "Author: %s  |  Version: %s  |  API: %s",
                script.author.empty() ? "Unknown" : script.author.c_str(),
                script.version.empty() ? "Unknown" : script.version.c_str(),
                script.apiVersion.empty() ? "Unknown" : script.apiVersion.c_str());
            if (!script.description.empty()) {
                ImGui::TextWrapped("%s", script.description.c_str());
            }

            if (!script.permissions.empty()) {
                std::string permissions;
                for (const auto& permission : script.permissions) {
                    if (!permissions.empty()) permissions += ", ";
                    permissions += permission;
                }
                ImGui::TextDisabled("Permissions: %s", permissions.c_str());
            }

            if (!script.lastError.empty()) {
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImVec4{0.92F, 0.30F, 0.34F, 1.0F});
                ImGui::TextWrapped("Error: %s", script.lastError.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PopID();
        }
    }
}

void Menu::RenderStyleEditor() {
    RenderInfoCard(
        "Changes apply immediately and remain active for this run. Reapply the "
        "selected Direct Menu theme at any time to recover its intended colors "
        "and spacing.");

    if (ImGui::Button("Save Current Style", {-1.0F, 38.0F})) {
        fonts_.SyncFromImGui();
        settings_.theme = std::string{themes_.Current()};
        settings_.font = std::string{fonts_.Current()};
        settings_.fontScale = fonts_.Scale();
        settings_.imguiStyle.Capture(ImGui::GetStyle());
        std::string error;
        if (configs_.Save(
                configurationName_.data(),
                settings_,
                features_,
                error)) {
            RefreshConfigurationNames();
            notifications_.Push(
                std::format(
                    "ImGui style saved to '{}'.",
                    configurationName_.data()),
                NotificationKind::Success);
        } else {
            notifications_.Push(error, NotificationKind::Error, 6.0);
        }
    }

    if (ImGui::Button("Reapply Direct Menu Theme", {-1.0F, 38.0F})) {
        if (themes_.Apply(settings_.theme)) {
            notifications_.Push(
                "Reapplied the selected Direct Menu theme.",
                NotificationKind::Success);
        } else {
            notifications_.Push(
                "The selected theme could not be reapplied.",
                NotificationKind::Error);
        }
    }

    if (ImGui::Button("Reset to ImGui Dark Defaults", {-1.0F, 38.0F})) {
        ImGui::StyleColorsDark(&ImGui::GetStyle());
        notifications_.Push(
            "Reset the editor to ImGui dark defaults. Use Reapply Direct Menu Theme to restore branding.",
            NotificationKind::Info,
            6.0);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::ShowStyleEditor(&ImGui::GetStyle());
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
    if (ImGui::SliderFloat("Live Font Scale", &scale, 0.30F, 2.00F, "%.2f")) {
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
        fonts_.SyncFromImGui();
        settings_.theme = std::string{themes_.Current()};
        settings_.font = std::string{fonts_.Current()};
        settings_.fontScale = fonts_.Scale();
        settings_.imguiStyle.Capture(ImGui::GetStyle());
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
        settings_.theme = "Reaper";
        themes_.Apply(settings_.theme);
    }
    settings_.imguiStyle.Apply(ImGui::GetStyle());

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

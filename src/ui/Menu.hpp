#pragma once

#include "core/AppSettings.hpp"

#include <Windows.h>

#include <array>
#include <functional>
#include <string_view>
#include <vector>

namespace smf::core {
class MemoryManagerAPI;
}

namespace smf::backend {
class D3D12Backend;
}

namespace smf::config {
class ConfigManager;
}

namespace smf::features {
class FeatureRegistry;
struct Feature;
}

namespace smf::filesystem {
class FileSystemManager;
}

namespace smf::scripting {
class LuaManager;
}

namespace smf::ui {

class FontManager;
class ImageLoader;
class NotificationCenter;
class ThemeManager;

enum class MenuPage {
    Home,
    Self,
    Weapons,
    Teleport,
    Unlocks,
    Vehicle,
    VehicleSpawn,
    Lsc,
    Network,
    OnlineSessions,
    Misc,
    Settings,
    StyleEditor,
    Lua,
    Themes,
    Images,
    Fonts,
    Configurations
};

struct MenuCallbacks {
    std::function<void()> toggleVisibility;
    std::function<void()> requestExit;
};

class Menu {
public:
    Menu(
        HWND window,
        backend::D3D12Backend& backend,
        config::ConfigManager& configs,
        features::FeatureRegistry& features,
        const filesystem::FileSystemManager& fileSystem,
        scripting::LuaManager& lua,
        core::MemoryManagerAPI& memory,
        FontManager& fonts,
        ImageLoader& brandBackground,
        ImageLoader& brandHeader,
        ImageLoader& brandIcon,
        ImageLoader& images,
        NotificationCenter& notifications,
        ThemeManager& themes,
        core::AppSettings& settings,
        MenuCallbacks callbacks);

    void RenderWelcome();
    void RenderMain();

    [[nodiscard]] bool WelcomeComplete() const noexcept;

private:
    void Navigate(MenuPage page);
    void Back();
    [[nodiscard]] MenuPage CurrentPage() const;
    [[nodiscard]] static std::string_view PageTitle(MenuPage page);
    [[nodiscard]] static std::string_view PageDescription(MenuPage page);

    bool SubmenuButton(std::string_view label, MenuPage target);
    void DrawAppliedBackground() const;
    void DrawNeonHeader() const;
    void DrawBottomHeader() const;
    void RenderInfoCard(std::string_view text) const;
    void RenderFeatureCategory(std::string_view category);
    void RenderFeature(features::Feature& feature);

    void RenderHome();
    void RenderSelf();
    void RenderWeapons();
    void RenderTeleport();
    void RenderUnlocks();
    void RenderVehicle();
    void RenderVehicleSpawn();
    void RenderLsc();
    void RenderNetwork();
    void RenderOnlineSessions();
    void RenderMisc();
    void RenderSettings();
    void RenderStyleEditor();
    void RenderLua();
    void RenderThemes();
    void RenderImages();
    void RenderFonts();
    void RenderConfigurations();
    void ApplyLoadedSettings();
    void RefreshConfigurationNames();

    HWND window_{nullptr};
    backend::D3D12Backend& backend_;
    config::ConfigManager& configs_;
    features::FeatureRegistry& features_;
    const filesystem::FileSystemManager& fileSystem_;
    scripting::LuaManager& lua_;
    core::MemoryManagerAPI& memory_;
    FontManager& fonts_;
    ImageLoader& brandBackground_;
    ImageLoader& brandHeader_;
    ImageLoader& brandIcon_;
    ImageLoader& images_;
    NotificationCenter& notifications_;
    ThemeManager& themes_;
    core::AppSettings& settings_;
    MenuCallbacks callbacks_;

    bool welcomeComplete_{false};
    std::vector<MenuPage> navigation_{MenuPage::Home};
    std::array<char, 96> searchText_{};
    std::array<char, 96> vehicleModel_{"example_model"};
    std::array<float, 3> teleportPosition_{0.0F, 0.0F, 0.0F};
    std::array<char, 512> imagePath_{};
    std::array<char, 64> configurationName_{"default"};
    std::vector<std::string> configurationNames_;
};

} // namespace smf::ui

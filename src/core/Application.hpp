#pragma once

#include "backend/D3D12Backend.hpp"
#include "config/ConfigManager.hpp"
#include "core/AppSettings.hpp"
#include "core/TaskQueue.hpp"
#include "features/FeatureRegistry.hpp"
#include "filesystem/FileSystemManager.hpp"
#include "logging/Logger.hpp"
#include "scripting/LuaManager.hpp"
#include "ui/FontManager.hpp"
#include "ui/ImageLoader.hpp"
#include "ui/NotificationCenter.hpp"
#include "ui/ThemeManager.hpp"

#include <Windows.h>

#include <memory>

namespace smf::ui {
class Menu;
}

namespace smf::core {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run(HINSTANCE instance, int showCommand);

private:
    static constexpr int MenuHotkeyId = 0x5F05;

    bool CreateApplicationWindow(HINSTANCE instance, std::string& errorMessage);
    bool InitializeGraphics(std::string& errorMessage);
    void InitializeConsole();
    void ShutdownConsole() noexcept;
    void RegisterBuiltInFeatures();
    void ToggleMenuVisibility();
    void RequestExit();
    void Cleanup();

    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);
    LRESULT HandleWindowMessage(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    HINSTANCE instance_{nullptr};
    HWND window_{nullptr};
    WNDCLASSEXW windowClass_{};

    bool classRegistered_{false};
    bool menuVisible_{true};
    bool hotkeyRegistered_{false};
    bool minimized_{false};
    bool imguiContextCreated_{false};
    bool consoleAllocated_{false};
    bool cleanedUp_{false};

    filesystem::FileSystemManager fileSystem_;
    logging::LoggerApi logger_;
    TaskQueue tasks_;
    features::FeatureRegistry features_;
    AppSettings settings_;
    backend::D3D12Backend backend_;
    config::ConfigManager configs_;
    scripting::LuaManager lua_;
    ui::ThemeManager themes_;
    ui::FontManager fonts_;
    ui::ImageLoader images_;
    ui::ImageLoader brandingIcon_;
    ui::NotificationCenter notifications_;
    std::unique_ptr<ui::Menu> menu_;
};

} // namespace smf::core

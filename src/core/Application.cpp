#include "core/Application.hpp"

#include "resources/resource.h"
#include "ui/Menu.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <format>
#include <iostream>
#include <string>
#include <thread>

// Dear ImGui intentionally leaves this callback declaration to the application
// because including Windows types from imgui_impl_win32.h would pollute users of
// that header.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

namespace smf::core {
namespace {

constexpr wchar_t WindowClassName[] = L"Direct_Menu.Window";
constexpr wchar_t WindowTitle[] = L"Direct_Menu";
constexpr int ClientWidth = 520;
constexpr int ClientHeight = 720;

} // namespace

Application::Application()
    : configs_(fileSystem_, logger_),
      lua_(logger_),
      fonts_(logger_),
      images_(logger_),
      brandingIcon_(logger_) {
}

Application::~Application() {
    Cleanup();
}

int Application::Run(const HINSTANCE instance, const int showCommand) {
    instance_ = instance;
    InitializeConsole();

    if (!fileSystem_.Initialize(L"Direct_Menu")) {
        MessageBoxW(
            nullptr,
            L"The runtime folders could not be created.",
            WindowTitle,
            MB_OK | MB_ICONERROR);
        return 1;
    }

    logger_.Initialize(fileSystem_.Logs() / L"latest.log");
    logger_.Info("Starting Direct_Menu.");
    tasks_.SetErrorHandler([this](const std::string_view message) {
        logger_.Error(std::string{"Background task failed: "} + std::string{message});
    });
    tasks_.Start(2);
    logger_.Info(
        "Started background task queue with " +
        std::to_string(tasks_.WorkerCount()) +
        " worker(s).");

    std::string errorMessage;
    if (!CreateApplicationWindow(instance, errorMessage)) {
        logger_.Critical(errorMessage);
        MessageBoxA(
            nullptr,
            errorMessage.c_str(),
            "Direct_Menu",
            MB_OK | MB_ICONERROR);
        Cleanup();
        return 2;
    }

    if (!InitializeGraphics(errorMessage)) {
        logger_.Critical(errorMessage);
        MessageBoxA(
            window_,
            errorMessage.c_str(),
            "Direct_Menu",
            MB_OK | MB_ICONERROR);
        Cleanup();
        return 3;
    }

    RegisterBuiltInFeatures();
    lua_.Initialize(fileSystem_.LuaScripts());
    lua_.BindFeatureRegistry(features_);

    ui::MenuCallbacks callbacks{};
    callbacks.toggleVisibility = [this] {
        ToggleMenuVisibility();
    };
    callbacks.requestExit = [this] {
        RequestExit();
    };

    menu_ = std::make_unique<ui::Menu>(
        window_,
        backend_,
        configs_,
        features_,
        fileSystem_,
        lua_,
        fonts_,
        images_,
        brandingIcon_,
        notifications_,
        themes_,
        settings_,
        std::move(callbacks));

    hotkeyRegistered_ =
        RegisterHotKey(window_, MenuHotkeyId, MOD_NOREPEAT, VK_F5) != FALSE;
    if (hotkeyRegistered_) {
        logger_.Info("Registered global F5 visibility hotkey.");
    } else {
        logger_.Warning(
            "The global F5 hotkey could not be registered; focused-window "
            "fallback remains available.");
    }

    ShowWindow(window_, showCommand == SW_HIDE ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(window_);

    bool done = false;
    while (!done) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                done = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (done) {
            break;
        }

        if (!menuVisible_ || minimized_) {
            WaitMessage();
            continue;
        }

        if (backend_.IsOccluded()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            continue;
        }

        backend_.NewFrame();
        ImGui::NewFrame();

        if (menu_->WelcomeComplete()) {
            menu_->RenderMain();
        } else {
            menu_->RenderWelcome();
        }
        notifications_.Render();

        ImGui::Render();
        constexpr float clearColor[4] = {0.018F, 0.024F, 0.034F, 1.0F};
        std::string renderError;
        if (!backend_.Render(ImGui::GetDrawData(), clearColor, renderError)) {
            logger_.Critical(renderError);
            const std::filesystem::path logPath =
                fileSystem_.Logs() / L"latest.log";
            const std::wstring dialogMessage =
                L"Direct3D 12 rendering failed.\n\n" +
                filesystem::FileSystemManager::ToWide(renderError) +
                L"\n\nLog file:\n" + logPath.wstring();
            MessageBoxW(
                window_,
                dialogMessage.c_str(),
                WindowTitle,
                MB_OK | MB_ICONERROR);
            done = true;
        }
    }

    Cleanup();
    return 0;
}

void Application::InitializeConsole() {
    if (GetConsoleWindow() == nullptr) {
        consoleAllocated_ = AllocConsole() != FALSE;
    }

    if (GetConsoleWindow() == nullptr) {
        return;
    }

    SetConsoleTitleW(L"Direct_Menu Logger");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    FILE* standardOutput = nullptr;
    FILE* standardError = nullptr;
    const bool outputReady =
        freopen_s(&standardOutput, "CONOUT$", "w", stdout) == 0;
    const bool errorReady =
        freopen_s(&standardError, "CONOUT$", "w", stderr) == 0;
    if (outputReady || errorReady) {
        std::ios::sync_with_stdio(true);
        std::cout.clear();
        std::cerr.clear();
        std::clog.clear();
        logger_.SetConsoleOutputEnabled(true);
    }
}

void Application::ShutdownConsole() noexcept {
    std::fflush(stdout);
    std::fflush(stderr);
    if (consoleAllocated_) {
        FreeConsole();
        consoleAllocated_ = false;
    }
}

bool Application::CreateApplicationWindow(
    const HINSTANCE instance,
    std::string& errorMessage) {
    ImGui_ImplWin32_EnableDpiAwareness();

    windowClass_ = {};
    windowClass_.cbSize = sizeof(windowClass_);
    windowClass_.style = CS_CLASSDC;
    windowClass_.lpfnWndProc = WindowProcedure;
    windowClass_.hInstance = instance;
    windowClass_.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass_.hIcon = static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_DIRECT_MENU),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE | LR_SHARED));
    windowClass_.hIconSm = static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_DIRECT_MENU),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_SHARED));
    windowClass_.lpszClassName = WindowClassName;

    if (RegisterClassExW(&windowClass_) == 0) {
        errorMessage = "RegisterClassExW failed.";
        return false;
    }
    classRegistered_ = true;

    const UINT dpi = GetDpiForSystem();
    const float scale = static_cast<float>(dpi) / 96.0F;

    RECT windowRectangle{
        0,
        0,
        static_cast<LONG>(ClientWidth * scale),
        static_cast<LONG>(ClientHeight * scale)
    };
    constexpr DWORD windowStyle =
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRectExForDpi(
        &windowRectangle,
        windowStyle,
        FALSE,
        WS_EX_APPWINDOW,
        dpi);

    const int width = windowRectangle.right - windowRectangle.left;
    const int height = windowRectangle.bottom - windowRectangle.top;
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int x = workArea.left + static_cast<int>(32.0F * scale);
    const int y = workArea.top + static_cast<int>(32.0F * scale);

    window_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        WindowClassName,
        WindowTitle,
        windowStyle,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        this);
    if (window_ == nullptr) {
        errorMessage = "CreateWindowExW failed.";
        return false;
    }

    errorMessage.clear();
    return true;
}

bool Application::InitializeGraphics(std::string& errorMessage) {
    if (!backend_.Initialize(window_, errorMessage)) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imguiContextCreated_ = true;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    fonts_.Initialize(fileSystem_.Fonts());
    fonts_.Select(settings_.font);
    fonts_.SetScale(settings_.fontScale);
    themes_.Apply(settings_.theme);

    if (!backend_.InitializeImGui(window_, errorMessage)) {
        return false;
    }

    std::array<wchar_t, 32768> executablePath{};
    const DWORD pathLength = GetModuleFileNameW(
        nullptr,
        executablePath.data(),
        static_cast<DWORD>(executablePath.size()));
    if (pathLength > 0 && pathLength < executablePath.size()) {
        const std::filesystem::path defaultBackground =
            std::filesystem::path{executablePath.data()}.parent_path() /
            L"assets" / L"direct_menu_neon_background.jpg";
        if (std::filesystem::exists(defaultBackground)) {
            std::string imageError;
            if (!images_.Load(defaultBackground, backend_, imageError)) {
                logger_.Warning(
                    "The included neon background could not be loaded: " +
                    imageError);
            }
        }

        const std::filesystem::path brandingIcon =
            std::filesystem::path{executablePath.data()}.parent_path() /
            L"assets" / L"grim_reaper_icon.png";
        if (std::filesystem::exists(brandingIcon)) {
            std::string imageError;
            if (!brandingIcon_.Load(brandingIcon, backend_, imageError)) {
                logger_.Warning(
                    "The Grim Reaper header icon could not be loaded: " +
                    imageError);
            }
        }
    }

    return true;
}

void Application::RegisterBuiltInFeatures() {
    features_.RegisterToggle(
        "self.invincibility",
        "Self",
        "Invincibility",
        "Example self-state toggle for the native provider API.");
    features_.RegisterToggle(
        "self.never_wanted",
        "Self",
        "Never Wanted",
        "Example wanted-state toggle for the native provider API.");
    features_.RegisterFloat(
        "self.sprint_multiplier",
        "Self",
        "Sprint Multiplier",
        "Example movement value stored by the typed feature registry.",
        1.0F,
        0.5F,
        3.0F,
        0.1F);

    features_.RegisterToggle(
        "weapons.unlimited_ammo",
        "Weapons",
        "Unlimited Ammo",
        "Example weapons toggle with no connected implementation.");
    features_.RegisterToggle(
        "weapons.no_recoil",
        "Weapons",
        "No Recoil",
        "Example recoil-state toggle with no connected implementation.");
    features_.RegisterFloat(
        "weapons.damage_multiplier",
        "Weapons",
        "Damage Multiplier",
        "Example weapons value stored by the typed feature registry.",
        1.0F,
        0.1F,
        5.0F,
        0.1F);

    features_.RegisterToggle(
        "unlocks.show_locked_items",
        "Unlocks",
        "Show Locked Items",
        "Local preview state only; it does not alter protected content.");
    features_.RegisterToggle(
        "unlocks.completion_preview",
        "Unlocks",
        "Completion Preview",
        "Local preview state only; it does not change account progress.");

    features_.RegisterToggle(
        "vehicle.invincibility",
        "Vehicle",
        "Vehicle Invincibility",
        "Example vehicle-state toggle for a future native provider.");
    features_.RegisterToggle(
        "vehicle.auto_repair",
        "Vehicle",
        "Auto Repair",
        "Example repair-state toggle for a future native provider.");
    features_.RegisterFloat(
        "vehicle.speed_limit",
        "Vehicle",
        "Speed Limit",
        "Example vehicle value stored by the typed feature registry.",
        120.0F,
        10.0F,
        400.0F,
        5.0F);

    features_.RegisterInteger(
        "lsc.primary_color",
        "LSC",
        "Primary Color",
        "Example indexed color value.",
        0,
        0,
        160);
    features_.RegisterInteger(
        "lsc.secondary_color",
        "LSC",
        "Secondary Color",
        "Example indexed color value.",
        0,
        0,
        160);
    features_.RegisterInteger(
        "lsc.wheel_type",
        "LSC",
        "Wheel Type",
        "Example wheel-category value.",
        0,
        0,
        12);
    features_.RegisterInteger(
        "lsc.window_tint",
        "LSC",
        "Window Tint",
        "Example window-tint value.",
        0,
        0,
        6);

    features_.RegisterToggle(
        "network.show_status",
        "Network",
        "Show Connection Status",
        "Display a local connection-status panel when a provider is added.");
    features_.RegisterToggle(
        "network.bandwidth_monitor",
        "Network",
        "Bandwidth Monitor",
        "Display local bandwidth diagnostics when a provider is added.");
    features_.RegisterInteger(
        "network.refresh_interval",
        "Network",
        "Refresh Interval (seconds)",
        "Saved refresh interval for future network diagnostics.",
        5,
        1,
        60);

    features_.RegisterToggle(
        "sessions.join_notifications",
        "Online Sessions",
        "Join Notifications",
        "Show a local notification for future session events.");
    features_.RegisterToggle(
        "sessions.friends_only",
        "Online Sessions",
        "Friends-Only Filter",
        "Saved filter preference for a future legitimate session provider.");
    features_.RegisterInteger(
        "sessions.maximum_results",
        "Online Sessions",
        "Maximum Results",
        "Saved display limit for future session-list results.",
        16,
        1,
        32);

    features_.RegisterToggle(
        "misc.show_clock",
        "Misc",
        "Show Menu Clock",
        "Saved preference for a clock in the menu interface.");
    features_.RegisterToggle(
        "misc.compact_notifications",
        "Misc",
        "Compact Notifications",
        "Saved preference for smaller notification cards.");
    features_.RegisterFloat(
        "misc.interface_scale",
        "Misc",
        "Interface Scale",
        "Saved interface scaling preference for a future layout provider.",
        1.0F,
        0.75F,
        1.50F,
        0.05F);

    logger_.Info("Registered built-in UI placeholder features.");
}

void Application::ToggleMenuVisibility() {
    menuVisible_ = !menuVisible_;

    if (window_ == nullptr) {
        return;
    }

    if (menuVisible_) {
        ShowWindow(window_, SW_SHOW);
        SetForegroundWindow(window_);
        logger_.Debug("Menu shown with F5.");
    } else {
        ShowWindow(window_, SW_HIDE);
        logger_.Debug("Menu hidden with F5.");
    }
}

void Application::RequestExit() {
    if (window_ != nullptr) {
        PostMessageW(window_, WM_CLOSE, 0, 0);
    }
}

void Application::Cleanup() {
    if (cleanedUp_) {
        return;
    }
    cleanedUp_ = true;

    if (hotkeyRegistered_ && window_ != nullptr) {
        UnregisterHotKey(window_, MenuHotkeyId);
        hotkeyRegistered_ = false;
    }

    tasks_.Shutdown();
    menu_.reset();
    if (images_.HasImage()) {
        images_.Clear(backend_);
    }
    if (brandingIcon_.HasImage()) {
        brandingIcon_.Clear(backend_);
    }

    if (imguiContextCreated_) {
        backend_.ShutdownImGui();
        ImGui::DestroyContext();
        imguiContextCreated_ = false;
    }
    backend_.Shutdown();

    if (window_ != nullptr && IsWindow(window_)) {
        DestroyWindow(window_);
    }
    window_ = nullptr;

    if (classRegistered_) {
        UnregisterClassW(WindowClassName, instance_);
        classRegistered_ = false;
    }

    logger_.Info("Direct_Menu stopped.");
    logger_.Shutdown();
    ShutdownConsole();
}

LRESULT CALLBACK Application::WindowProcedure(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam) {
    Application* application = reinterpret_cast<Application*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        application = static_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtrW(
            window,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(application));
        application->window_ = window;
    }

    if (application != nullptr) {
        return application->HandleWindowMessage(
            window,
            message,
            wParam,
            lParam);
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT Application::HandleWindowMessage(
    const HWND window,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam) {
    if (imguiContextCreated_ &&
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
        return TRUE;
    }

    switch (message) {
    case WM_HOTKEY:
        if (static_cast<int>(wParam) == MenuHotkeyId) {
            ToggleMenuVisibility();
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (!hotkeyRegistered_ && wParam == VK_F5) {
            ToggleMenuVisibility();
            return 0;
        }
        break;

    case WM_SIZE:
        minimized_ = wParam == SIZE_MINIMIZED;
        if (!minimized_ && backend_.IsInitialized()) {
            backend_.Resize(
                static_cast<std::uint32_t>(LOWORD(lParam)),
                static_cast<std::uint32_t>(HIWORD(lParam)));
        }
        return 0;

    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(
            window,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        return 0;
    }

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0U) == SC_KEYMENU) {
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_DESTROY:
        window_ = nullptr;
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace smf::core

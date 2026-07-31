#include "filesystem/FileSystemManager.hpp"

#include <ShlObj.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <system_error>

namespace smf::filesystem {

bool FileSystemManager::Initialize(const std::wstring_view applicationName) {
    PWSTR localAppData = nullptr;
    const HRESULT folderResult = SHGetKnownFolderPath(
        FOLDERID_LocalAppData,
        KF_FLAG_CREATE,
        nullptr,
        &localAppData);

    if (SUCCEEDED(folderResult) && localAppData != nullptr) {
        root_ =
            std::filesystem::path{localAppData} /
            std::wstring{applicationName};
        CoTaskMemFree(localAppData);
    } else {
        root_ = std::filesystem::current_path() / applicationName;
    }

    configurations_ = root_ / L"Configurations";
    fonts_ = root_ / L"Fonts";
    images_ = root_ / L"Images";
    logs_ = root_ / L"Logs";
    luaScripts_ = root_ / L"LuaScripts";

    std::error_code error;
    std::filesystem::create_directories(configurations_, error);
    if (error) {
        return false;
    }
    std::filesystem::create_directories(fonts_, error);
    if (error) {
        return false;
    }
    std::filesystem::create_directories(images_, error);
    if (error) {
        return false;
    }
    std::filesystem::create_directories(logs_, error);
    if (error) {
        return false;
    }
    std::filesystem::create_directories(luaScripts_, error);
    return !error;
}

const std::filesystem::path& FileSystemManager::Root() const noexcept {
    return root_;
}

const std::filesystem::path& FileSystemManager::Configurations() const noexcept {
    return configurations_;
}

const std::filesystem::path& FileSystemManager::Fonts() const noexcept {
    return fonts_;
}

const std::filesystem::path& FileSystemManager::Images() const noexcept {
    return images_;
}

const std::filesystem::path& FileSystemManager::Logs() const noexcept {
    return logs_;
}

const std::filesystem::path& FileSystemManager::LuaScripts() const noexcept {
    return luaScripts_;
}

std::filesystem::path FileSystemManager::ConfigurationPath(const std::string_view name) const {
    return configurations_ / (SanitizeFileStem(name) + ".ini");
}

std::vector<std::string> FileSystemManager::ConfigurationNames() const {
    std::vector<std::string> names;
    std::error_code error;

    if (!std::filesystem::exists(configurations_, error)) {
        return names;
    }

    for (const auto& entry : std::filesystem::directory_iterator(configurations_, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file(error) && entry.path().extension() == L".ini") {
            names.push_back(ToUtf8(entry.path().stem().wstring()));
        }
    }

    std::ranges::sort(names);
    return names;
}

std::optional<std::filesystem::path> FileSystemManager::OpenImageDialog(const HWND owner) const {
    std::array<wchar_t, 1024> pathBuffer{};
    const std::wstring initialDirectory = images_.wstring();

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter =
        L"Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff)\0"
        L"*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0"
        L"All Files (*.*)\0"
        L"*.*\0\0";
    dialog.lpstrFile = pathBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(pathBuffer.size());
    dialog.lpstrInitialDir = initialDirectory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&dialog) == FALSE) {
        return std::nullopt;
    }

    return std::filesystem::path{pathBuffer.data()};
}

std::string FileSystemManager::SanitizeFileStem(const std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());

    for (const char character : value) {
        const bool alphaNumeric =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9');
        if (alphaNumeric || character == '-' || character == '_') {
            sanitized.push_back(character);
        } else if (character == ' ') {
            sanitized.push_back('_');
        }
    }

    if (sanitized.empty()) {
        return "default";
    }

    constexpr std::size_t maximumLength = 64;
    if (sanitized.size() > maximumLength) {
        sanitized.resize(maximumLength);
    }
    return sanitized;
}

std::string FileSystemManager::ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string converted(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        converted.data(),
        required,
        nullptr,
        nullptr);
    return converted;
}

std::wstring FileSystemManager::ToWide(const std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (required <= 0) {
        return {};
    }

    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        converted.data(),
        required);
    return converted;
}

} // namespace smf::filesystem

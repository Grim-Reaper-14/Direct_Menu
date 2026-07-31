#pragma once

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace smf::filesystem {

class FileSystemManager {
public:
    bool Initialize(std::wstring_view applicationName);

    [[nodiscard]] const std::filesystem::path& Root() const noexcept;
    [[nodiscard]] const std::filesystem::path& Configurations() const noexcept;
    [[nodiscard]] const std::filesystem::path& Fonts() const noexcept;
    [[nodiscard]] const std::filesystem::path& Images() const noexcept;
    [[nodiscard]] const std::filesystem::path& Logs() const noexcept;
    [[nodiscard]] const std::filesystem::path& LuaScripts() const noexcept;

    [[nodiscard]] std::filesystem::path ConfigurationPath(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> ConfigurationNames() const;
    [[nodiscard]] std::optional<std::filesystem::path> OpenImageDialog(HWND owner) const;

    [[nodiscard]] static std::string SanitizeFileStem(std::string_view value);
    [[nodiscard]] static std::string ToUtf8(const std::wstring& value);
    [[nodiscard]] static std::wstring ToWide(std::string_view value);

private:
    std::filesystem::path root_;
    std::filesystem::path configurations_;
    std::filesystem::path fonts_;
    std::filesystem::path images_;
    std::filesystem::path logs_;
    std::filesystem::path luaScripts_;
};

} // namespace smf::filesystem


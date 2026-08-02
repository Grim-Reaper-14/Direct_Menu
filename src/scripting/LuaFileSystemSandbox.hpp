#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace smf::logging {
class LoggerApi;
}

namespace smf::scripting {

class LuaFileSystemSandbox final {
public:
    explicit LuaFileSystemSandbox(logging::LoggerApi& logger);

    bool Initialize(std::filesystem::path rootDirectory);

    [[nodiscard]] std::filesystem::path ScriptRoot(std::string_view owner) const;
    [[nodiscard]] bool Exists(std::string_view owner, std::string_view relativePath) const;
    [[nodiscard]] bool IsDirectory(std::string_view owner, std::string_view relativePath) const;

    [[nodiscard]] std::optional<std::string> ReadText(
        std::string_view owner,
        std::string_view relativePath,
        std::size_t maximumBytes = 1024U * 1024U) const;

    bool WriteText(
        std::string_view owner,
        std::string_view relativePath,
        std::string_view contents,
        std::size_t maximumBytes = 1024U * 1024U);

    bool AppendText(
        std::string_view owner,
        std::string_view relativePath,
        std::string_view contents,
        std::size_t maximumBytes = 1024U * 1024U);

    bool CreateDirectory(std::string_view owner, std::string_view relativePath);
    bool Remove(std::string_view owner, std::string_view relativePath);

    [[nodiscard]] std::vector<std::string> List(
        std::string_view owner,
        std::string_view relativePath = {}) const;

    [[nodiscard]] const std::filesystem::path& Root() const noexcept;

private:
    [[nodiscard]] std::optional<std::filesystem::path> Resolve(
        std::string_view owner,
        std::string_view relativePath,
        bool createOwnerDirectory) const;

    [[nodiscard]] static std::string SanitizeOwner(std::string_view owner);
    [[nodiscard]] static bool IsContained(
        const std::filesystem::path& root,
        const std::filesystem::path& candidate);

    logging::LoggerApi& logger_;
    std::filesystem::path rootDirectory_;
};

} // namespace smf::scripting

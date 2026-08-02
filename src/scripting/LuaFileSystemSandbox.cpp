#include "scripting/LuaFileSystemSandbox.hpp"

#include "logging/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>

namespace smf::scripting {

LuaFileSystemSandbox::LuaFileSystemSandbox(logging::LoggerApi& logger)
    : logger_(logger) {
}

bool LuaFileSystemSandbox::Initialize(std::filesystem::path rootDirectory) {
    std::error_code error;
    rootDirectory_ = std::filesystem::absolute(std::move(rootDirectory), error).lexically_normal();
    if (error) {
        logger_.Error("Lua filesystem sandbox root could not be resolved: " + error.message());
        return false;
    }
    std::filesystem::create_directories(rootDirectory_, error);
    if (error) {
        logger_.Error("Lua filesystem sandbox root could not be created: " + error.message());
        return false;
    }
    rootDirectory_ = std::filesystem::weakly_canonical(rootDirectory_, error);
    if (error) {
        logger_.Error("Lua filesystem sandbox root could not be canonicalized: " + error.message());
        return false;
    }
    logger_.Info("Lua filesystem sandbox initialized: " + rootDirectory_.string());
    return true;
}

std::filesystem::path LuaFileSystemSandbox::ScriptRoot(const std::string_view owner) const {
    return (rootDirectory_ / SanitizeOwner(owner)).lexically_normal();
}

bool LuaFileSystemSandbox::Exists(const std::string_view owner, const std::string_view relativePath) const {
    const auto resolved = Resolve(owner, relativePath, false);
    if (!resolved.has_value()) return false;
    std::error_code error;
    return std::filesystem::exists(*resolved, error) && !error;
}

bool LuaFileSystemSandbox::IsDirectory(const std::string_view owner, const std::string_view relativePath) const {
    const auto resolved = Resolve(owner, relativePath, false);
    if (!resolved.has_value()) return false;
    std::error_code error;
    return std::filesystem::is_directory(*resolved, error) && !error;
}

std::optional<std::string> LuaFileSystemSandbox::ReadText(
    const std::string_view owner,
    const std::string_view relativePath,
    const std::size_t maximumBytes) const {
    const auto resolved = Resolve(owner, relativePath, false);
    if (!resolved.has_value()) return std::nullopt;
    std::error_code error;
    const auto size = std::filesystem::file_size(*resolved, error);
    if (error || size > maximumBytes) return std::nullopt;
    std::ifstream input(*resolved, std::ios::binary);
    if (!input) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool LuaFileSystemSandbox::WriteText(
    const std::string_view owner,
    const std::string_view relativePath,
    const std::string_view contents,
    const std::size_t maximumBytes) {
    if (contents.size() > maximumBytes) {
        logger_.Warning("Lua sandbox write exceeded the configured size limit.");
        return false;
    }
    const auto resolved = Resolve(owner, relativePath, true);
    if (!resolved.has_value()) {
        logger_.Warning(
            "Lua sandbox write path was rejected for '" +
            std::string{owner} + "': " + std::string{relativePath});
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(resolved->parent_path(), error);
    if (error) {
        logger_.Warning(
            "Lua sandbox write directory could not be created: " +
            error.message());
        return false;
    }
    const auto checked = Resolve(owner, relativePath, false);
    if (!checked.has_value()) {
        logger_.Warning(
            "Lua sandbox write path failed its post-create containment check for '" +
            std::string{owner} + "': " + std::string{relativePath});
        return false;
    }
    std::ofstream output(*checked, std::ios::binary | std::ios::trunc);
    if (!output) {
        logger_.Warning(
            "Lua sandbox file could not be opened for writing: " +
            checked->string());
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return output.good();
}

bool LuaFileSystemSandbox::AppendText(
    const std::string_view owner,
    const std::string_view relativePath,
    const std::string_view contents,
    const std::size_t maximumBytes) {
    const auto resolved = Resolve(owner, relativePath, true);
    if (!resolved.has_value()) return false;
    std::error_code error;
    std::filesystem::create_directories(resolved->parent_path(), error);
    if (error) return false;
    const auto checked = Resolve(owner, relativePath, false);
    if (!checked.has_value()) return false;
    std::uintmax_t currentSize = 0;
    if (std::filesystem::exists(*checked, error) && !error) {
        currentSize = std::filesystem::file_size(*checked, error);
        if (error) return false;
    }
    if (currentSize + contents.size() > maximumBytes) return false;
    std::ofstream output(*checked, std::ios::binary | std::ios::app);
    if (!output) return false;
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return output.good();
}

bool LuaFileSystemSandbox::CreateDirectory(const std::string_view owner, const std::string_view relativePath) {
    const auto resolved = Resolve(owner, relativePath, true);
    if (!resolved.has_value()) return false;
    std::error_code error;
    const bool created = std::filesystem::create_directories(*resolved, error);
    if (error) return false;
    const auto checked = Resolve(owner, relativePath, false);
    return checked.has_value() && (created || std::filesystem::is_directory(*checked, error));
}

bool LuaFileSystemSandbox::Remove(const std::string_view owner, const std::string_view relativePath) {
    if (relativePath.empty()) return false;
    const auto resolved = Resolve(owner, relativePath, false);
    if (!resolved.has_value()) return false;
    std::error_code error;
    return std::filesystem::remove_all(*resolved, error) > 0 && !error;
}

std::vector<std::string> LuaFileSystemSandbox::List(
    const std::string_view owner,
    const std::string_view relativePath) const {
    std::vector<std::string> entries;
    const auto resolved = Resolve(owner, relativePath, false);
    if (!resolved.has_value()) return entries;
    std::error_code error;
    if (!std::filesystem::is_directory(*resolved, error) || error) return entries;
    for (const auto& entry : std::filesystem::directory_iterator(*resolved, error)) {
        if (error) break;
        entries.push_back(entry.path().filename().string());
    }
    std::ranges::sort(entries);
    return entries;
}

const std::filesystem::path& LuaFileSystemSandbox::Root() const noexcept { return rootDirectory_; }

std::optional<std::filesystem::path> LuaFileSystemSandbox::Resolve(
    const std::string_view owner,
    const std::string_view relativePath,
    const bool createOwnerDirectory) const {
    if (rootDirectory_.empty()) return std::nullopt;

    const std::filesystem::path requested{std::string{relativePath}};
    if (requested.is_absolute() || requested.has_root_name() || requested.has_root_directory()) return std::nullopt;
    for (const auto& component : requested) {
        if (component == "..") return std::nullopt;
    }

    const std::filesystem::path ownerRoot = ScriptRoot(owner);
    std::error_code error;
    if (createOwnerDirectory) {
        std::filesystem::create_directories(ownerRoot, error);
        if (error) return std::nullopt;
    }

    if (!std::filesystem::exists(ownerRoot, error)) {
        if (error || !createOwnerDirectory) return std::nullopt;
    }

    const std::filesystem::path canonicalOwner = std::filesystem::weakly_canonical(ownerRoot, error);
    if (error) {
        logger_.Warning(
            "Lua sandbox owner root could not be canonicalized: " +
            error.message());
        return std::nullopt;
    }

    const std::filesystem::path candidate =
        (ownerRoot / requested).lexically_normal();

    // MSVC's weakly_canonical can report access denied for a non-existent
    // final component. Resolve the longest existing prefix instead, then
    // append only the already-validated relative tail. Existing junctions
    // and symlinks are still canonicalized before the containment check.
    std::filesystem::path existingPrefix = candidate;
    while (existingPrefix != ownerRoot &&
           !std::filesystem::exists(existingPrefix, error)) {
        if (error) {
            logger_.Warning(
                "Lua sandbox path prefix could not be inspected: " +
                error.message());
            return std::nullopt;
        }
        existingPrefix = existingPrefix.parent_path();
    }
    if (error) {
        logger_.Warning(
            "Lua sandbox path prefix could not be inspected: " +
            error.message());
        return std::nullopt;
    }

    const std::filesystem::path canonicalPrefix =
        std::filesystem::weakly_canonical(existingPrefix, error);
    if (error) {
        logger_.Warning(
            "Lua sandbox path prefix could not be canonicalized: " +
            error.message());
        return std::nullopt;
    }
    if (!IsContained(canonicalOwner, canonicalPrefix)) {
        logger_.Warning(
            "Lua sandbox path prefix escaped its owner root: " +
            canonicalPrefix.string());
        return std::nullopt;
    }

    const std::filesystem::path relativeTail =
        candidate.lexically_relative(existingPrefix);
    const std::filesystem::path canonicalCandidate =
        (relativeTail.empty() || relativeTail == ".")
            ? canonicalPrefix
            : (canonicalPrefix / relativeTail).lexically_normal();
    if (!IsContained(canonicalOwner, canonicalCandidate)) {
        logger_.Warning(
            "Lua sandbox candidate escaped its owner root: " +
            canonicalCandidate.string());
        return std::nullopt;
    }

    return canonicalCandidate;
}

std::string LuaFileSystemSandbox::SanitizeOwner(const std::string_view owner) {
    std::string result;
    result.reserve(owner.size());
    for (const char character : owner) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (std::isalnum(value) != 0 || character == '-' || character == '_') result.push_back(character);
        else result.push_back('_');
    }
    if (result.empty() || result == "__native__") result = "native";
    return result;
}

bool LuaFileSystemSandbox::IsContained(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    const auto normalizedRoot = root.lexically_normal();
    const auto normalizedCandidate = candidate.lexically_normal();
    auto rootIt = normalizedRoot.begin();
    auto candidateIt = normalizedCandidate.begin();
    for (; rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == normalizedCandidate.end() || *rootIt != *candidateIt) return false;
    }
    return true;
}

} // namespace smf::scripting

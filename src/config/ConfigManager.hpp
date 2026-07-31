#pragma once

#include "core/AppSettings.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace smf::features {
class FeatureRegistry;
}

namespace smf::filesystem {
class FileSystemManager;
}

namespace smf::logging {
class LoggerApi;
}

namespace smf::config {

class ConfigManager {
public:
    ConfigManager(
        const filesystem::FileSystemManager& fileSystem,
        logging::LoggerApi& logger);

    bool Save(
        std::string_view name,
        const core::AppSettings& settings,
        const features::FeatureRegistry& registry,
        std::string& errorMessage) const;

    bool Load(
        std::string_view name,
        core::AppSettings& settings,
        features::FeatureRegistry& registry,
        std::string& errorMessage) const;

    [[nodiscard]] std::vector<std::string> Available() const;

private:
    const filesystem::FileSystemManager& fileSystem_;
    logging::LoggerApi& logger_;
};

} // namespace smf::config


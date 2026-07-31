#include "config/ConfigManager.hpp"

#include "features/FeatureRegistry.hpp"
#include "filesystem/FileSystemManager.hpp"
#include "logging/Logger.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace smf::config {
namespace {

std::string Trim(std::string value) {
    const auto notWhitespace = [](const unsigned char character) {
        return character != ' ' && character != '\t' &&
               character != '\r' && character != '\n';
    };

    const auto first = std::find_if(value.begin(), value.end(), notWhitespace);
    const auto last = std::find_if(value.rbegin(), value.rend(), notWhitespace).base();
    if (first >= last) {
        return {};
    }
    return std::string{first, last};
}

} // namespace

ConfigManager::ConfigManager(
    const filesystem::FileSystemManager& fileSystem,
    logging::LoggerApi& logger)
    : fileSystem_(fileSystem),
      logger_(logger) {
}

bool ConfigManager::Save(
    const std::string_view name,
    const core::AppSettings& settings,
    const features::FeatureRegistry& registry,
    std::string& errorMessage) const {
    const auto path = fileSystem_.ConfigurationPath(name);
    std::ofstream output{path, std::ios::out | std::ios::trunc};

    if (!output.is_open()) {
        errorMessage = "Could not open the configuration file for writing.";
        logger_.Error(errorMessage);
        return false;
    }

    output << "# Direct_Menu configuration\n";
    output << "version=1\n";
    output << "theme=" << settings.theme << '\n';
    output << "font=" << settings.font << '\n';
    output << "font_scale=" << std::fixed << std::setprecision(3)
           << settings.fontScale << '\n';
    output << "image_path=" << settings.imagePath << '\n';

    const auto registryValues = registry.SaveableValues();
    const std::map<std::string, std::string> sortedValues{
        registryValues.begin(),
        registryValues.end()};

    for (const auto& [id, value] : sortedValues) {
        output << "feature." << id << '=' << value << '\n';
    }

    output.flush();
    if (!output.good()) {
        errorMessage = "The configuration file could not be completely written.";
        logger_.Error(errorMessage);
        return false;
    }

    logger_.Info("Saved configuration: " + path.string());
    errorMessage.clear();
    return true;
}

bool ConfigManager::Load(
    const std::string_view name,
    core::AppSettings& settings,
    features::FeatureRegistry& registry,
    std::string& errorMessage) const {
    const auto path = fileSystem_.ConfigurationPath(name);
    std::ifstream input{path};

    if (!input.is_open()) {
        errorMessage = "The selected configuration file does not exist.";
        logger_.Warning(errorMessage);
        return false;
    }

    std::string line;
    std::size_t ignoredValues = 0;

    while (std::getline(input, line)) {
        line = Trim(std::move(line));
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            ++ignoredValues;
            continue;
        }

        const std::string key = Trim(line.substr(0, separator));
        const std::string value = Trim(line.substr(separator + 1));

        if (key == "theme") {
            settings.theme = value;
        } else if (key == "font") {
            settings.font = value;
        } else if (key == "font_scale") {
            try {
                settings.fontScale = std::clamp(std::stof(value), 0.75F, 1.50F);
            } catch (...) {
                ++ignoredValues;
            }
        } else if (key == "image_path") {
            settings.imagePath = value;
        } else if (key.starts_with("feature.")) {
            if (!registry.ApplyValue(key.substr(8), value)) {
                ++ignoredValues;
            }
        }
    }

    if (!input.eof() && input.fail()) {
        errorMessage = "The configuration file could not be completely read.";
        logger_.Error(errorMessage);
        return false;
    }

    std::ostringstream message;
    message << "Loaded configuration: " << path.string();
    if (ignoredValues > 0) {
        message << " (" << ignoredValues << " unsupported value(s) ignored)";
    }
    logger_.Info(message.str());

    errorMessage.clear();
    return true;
}

std::vector<std::string> ConfigManager::Available() const {
    return fileSystem_.ConfigurationNames();
}

} // namespace smf::config

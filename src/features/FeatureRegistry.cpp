#include "features/FeatureRegistry.hpp"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace smf::features {

bool FeatureRegistry::RegisterToggle(
    std::string id,
    std::string category,
    std::string label,
    std::string description,
    const bool defaultValue,
    const bool placeholder) {
    Feature feature{};
    feature.id = std::move(id);
    feature.category = std::move(category);
    feature.label = std::move(label);
    feature.description = std::move(description);
    feature.kind = FeatureKind::Toggle;
    feature.value = defaultValue;
    feature.defaultValue = defaultValue;
    feature.placeholder = placeholder;
    return Add(std::move(feature));
}

bool FeatureRegistry::RegisterInteger(
    std::string id,
    std::string category,
    std::string label,
    std::string description,
    const int defaultValue,
    const int minimum,
    const int maximum,
    const int step,
    const bool placeholder) {
    Feature feature{};
    feature.id = std::move(id);
    feature.category = std::move(category);
    feature.label = std::move(label);
    feature.description = std::move(description);
    feature.kind = FeatureKind::Integer;
    feature.value = std::clamp(defaultValue, minimum, maximum);
    feature.defaultValue = feature.value;
    feature.minimum = static_cast<float>(minimum);
    feature.maximum = static_cast<float>(maximum);
    feature.step = static_cast<float>(std::max(step, 1));
    feature.placeholder = placeholder;
    return Add(std::move(feature));
}

bool FeatureRegistry::RegisterFloat(
    std::string id,
    std::string category,
    std::string label,
    std::string description,
    const float defaultValue,
    const float minimum,
    const float maximum,
    const float step,
    const bool placeholder) {
    Feature feature{};
    feature.id = std::move(id);
    feature.category = std::move(category);
    feature.label = std::move(label);
    feature.description = std::move(description);
    feature.kind = FeatureKind::Float;
    feature.value = std::clamp(defaultValue, minimum, maximum);
    feature.defaultValue = feature.value;
    feature.minimum = minimum;
    feature.maximum = maximum;
    feature.step = std::max(step, 0.001F);
    feature.placeholder = placeholder;
    return Add(std::move(feature));
}

Feature* FeatureRegistry::Find(const std::string_view id) {
    const auto found = index_.find(std::string{id});
    return found == index_.end() ? nullptr : found->second;
}

const Feature* FeatureRegistry::Find(const std::string_view id) const {
    const auto found = index_.find(std::string{id});
    return found == index_.end() ? nullptr : found->second;
}

std::vector<Feature*> FeatureRegistry::InCategory(const std::string_view category) {
    std::vector<Feature*> result;
    for (auto& feature : features_) {
        if (feature.category == category) {
            result.push_back(&feature);
        }
    }
    return result;
}

std::vector<const Feature*> FeatureRegistry::InCategory(const std::string_view category) const {
    std::vector<const Feature*> result;
    for (const auto& feature : features_) {
        if (feature.category == category) {
            result.push_back(&feature);
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> FeatureRegistry::SaveableValues() const {
    std::unordered_map<std::string, std::string> result;

    for (const auto& feature : features_) {
        std::string serialized;

        switch (feature.kind) {
        case FeatureKind::Toggle:
            serialized = std::get<bool>(feature.value) ? "true" : "false";
            break;
        case FeatureKind::Integer:
            serialized = std::to_string(std::get<int>(feature.value));
            break;
        case FeatureKind::Float: {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << std::get<float>(feature.value);
            serialized = stream.str();
            break;
        }
        }

        result.emplace(feature.id, std::move(serialized));
    }

    return result;
}

bool FeatureRegistry::ApplyValue(
    const std::string_view id,
    const std::string_view serializedValue) {
    Feature* feature = Find(id);
    if (feature == nullptr) {
        return false;
    }

    switch (feature->kind) {
    case FeatureKind::Toggle:
        if (serializedValue == "true" || serializedValue == "1") {
            feature->value = true;
            return true;
        }
        if (serializedValue == "false" || serializedValue == "0") {
            feature->value = false;
            return true;
        }
        return false;

    case FeatureKind::Integer: {
        int parsed = 0;
        const auto* begin = serializedValue.data();
        const auto* end = serializedValue.data() + serializedValue.size();
        const auto conversion = std::from_chars(begin, end, parsed);
        if (conversion.ec != std::errc{} || conversion.ptr != end) {
            return false;
        }
        feature->value = std::clamp(
            parsed,
            static_cast<int>(feature->minimum),
            static_cast<int>(feature->maximum));
        return true;
    }

    case FeatureKind::Float: {
        float parsed = 0.0F;
        const auto* begin = serializedValue.data();
        const auto* end = serializedValue.data() + serializedValue.size();
        const auto conversion = std::from_chars(begin, end, parsed);
        if (conversion.ec != std::errc{} || conversion.ptr != end) {
            return false;
        }
        feature->value = std::clamp(parsed, feature->minimum, feature->maximum);
        return true;
    }
    }

    return false;
}

void FeatureRegistry::ResetToDefaults() {
    for (auto& feature : features_) {
        feature.value = feature.defaultValue;
    }
}

bool FeatureRegistry::Add(Feature feature) {
    if (feature.id.empty() || index_.contains(feature.id)) {
        return false;
    }

    features_.push_back(std::move(feature));
    Feature& stored = features_.back();
    index_.emplace(stored.id, &stored);
    return true;
}

} // namespace smf::features


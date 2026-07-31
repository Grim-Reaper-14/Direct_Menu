#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace smf::features {

enum class FeatureKind {
    Toggle,
    Integer,
    Float
};

using FeatureValue = std::variant<bool, int, float>;

struct Feature {
    std::string id;
    std::string category;
    std::string label;
    std::string description;
    FeatureKind kind{FeatureKind::Toggle};
    FeatureValue value{false};
    FeatureValue defaultValue{false};
    float minimum{0.0F};
    float maximum{1.0F};
    float step{1.0F};
    bool placeholder{true};
};

class FeatureRegistry {
public:
    bool RegisterToggle(
        std::string id,
        std::string category,
        std::string label,
        std::string description,
        bool defaultValue = false,
        bool placeholder = true);

    bool RegisterInteger(
        std::string id,
        std::string category,
        std::string label,
        std::string description,
        int defaultValue,
        int minimum,
        int maximum,
        int step = 1,
        bool placeholder = true);

    bool RegisterFloat(
        std::string id,
        std::string category,
        std::string label,
        std::string description,
        float defaultValue,
        float minimum,
        float maximum,
        float step = 0.1F,
        bool placeholder = true);

    [[nodiscard]] Feature* Find(std::string_view id);
    [[nodiscard]] const Feature* Find(std::string_view id) const;
    [[nodiscard]] std::vector<Feature*> InCategory(std::string_view category);
    [[nodiscard]] std::vector<const Feature*> InCategory(std::string_view category) const;

    [[nodiscard]] std::unordered_map<std::string, std::string> SaveableValues() const;
    bool ApplyValue(std::string_view id, std::string_view serializedValue);
    void ResetToDefaults();

private:
    bool Add(Feature feature);

    std::deque<Feature> features_;
    std::unordered_map<std::string, Feature*> index_;
};

} // namespace smf::features


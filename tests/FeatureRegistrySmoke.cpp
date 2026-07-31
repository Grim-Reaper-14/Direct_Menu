#include "features/FeatureRegistry.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <variant>

int main() {
    smf::features::FeatureRegistry registry;

    assert(registry.RegisterToggle(
        "self.example",
        "Self",
        "Example Toggle",
        "Smoke-test toggle."));
    assert(registry.RegisterInteger(
        "vehicle.color",
        "Vehicle",
        "Color",
        "Smoke-test integer.",
        10,
        0,
        160));
    assert(registry.RegisterFloat(
        "self.scale",
        "Self",
        "Scale",
        "Smoke-test float.",
        1.0F,
        0.5F,
        3.0F));

    assert(!registry.RegisterToggle(
        "self.example",
        "Self",
        "Duplicate",
        "Duplicate IDs must be rejected."));

    assert(registry.ApplyValue("self.example", "true"));
    assert(registry.ApplyValue("vehicle.color", "200"));
    assert(registry.ApplyValue("self.scale", "2.25"));

    const auto* toggle = registry.Find("self.example");
    const auto* color = registry.Find("vehicle.color");
    const auto* scale = registry.Find("self.scale");
    assert(toggle != nullptr && std::get<bool>(toggle->value));
    assert(color != nullptr && std::get<int>(color->value) == 160);
    assert(scale != nullptr && std::fabs(std::get<float>(scale->value) - 2.25F) < 0.001F);
    assert(registry.InCategory("Self").size() == 2);

    const auto saved = registry.SaveableValues();
    assert(saved.at("self.example") == "true");
    assert(saved.at("vehicle.color") == "160");

    registry.ResetToDefaults();
    assert(!std::get<bool>(registry.Find("self.example")->value));
    assert(std::get<int>(registry.Find("vehicle.color")->value) == 10);

    return 0;
}


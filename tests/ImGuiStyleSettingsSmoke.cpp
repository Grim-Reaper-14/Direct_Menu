#include "core/ImGuiStyleSettings.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

bool NearlyEqual(const float left, const float right) {
    return std::abs(left - right) < 0.0001F;
}

bool Expect(const bool condition, const std::string_view message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

} // namespace

int main() {
    ImGuiStyle source;
    source.WindowPadding = {23.0F, 17.0F};
    source.FrameRounding = 11.5F;
    source.AntiAliasedLines = false;
    source.HoverDelayNormal = 1.25F;
    source.Colors[ImGuiCol_Button] = {0.12F, 0.34F, 0.56F, 0.78F};

    smf::core::ImGuiStyleSettings saved;
    saved.Capture(source);
    std::ostringstream serialized;
    saved.Write(serialized);

    smf::core::ImGuiStyleSettings loaded;
    std::istringstream input{serialized.str()};
    std::string line;
    bool parsed = true;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || !line.starts_with("imgui.")) {
            parsed = false;
            continue;
        }
        parsed &= loaded.SetValue(
            std::string_view{line}.substr(6, separator - 6),
            std::string_view{line}.substr(separator + 1));
    }

    ImGuiStyle restored;
    loaded.Apply(restored);

    bool passed = true;
    passed &= Expect(parsed, "A serialized ImGui style value failed to parse.");
    passed &= Expect(loaded.HasValue(), "Loaded style was not marked valid.");
    passed &= Expect(
        NearlyEqual(restored.WindowPadding.x, 23.0F) &&
            NearlyEqual(restored.WindowPadding.y, 17.0F),
        "Window padding did not survive style persistence.");
    passed &= Expect(
        NearlyEqual(restored.FrameRounding, 11.5F),
        "Frame rounding did not survive style persistence.");
    passed &= Expect(
        !restored.AntiAliasedLines,
        "Boolean rendering settings did not survive style persistence.");
    passed &= Expect(
        NearlyEqual(restored.HoverDelayNormal, 1.25F),
        "Behavior settings did not survive style persistence.");
    passed &= Expect(
        NearlyEqual(restored.Colors[ImGuiCol_Button].x, 0.12F) &&
            NearlyEqual(restored.Colors[ImGuiCol_Button].w, 0.78F),
        "ImGui colors did not survive style persistence.");
    return passed ? 0 : 1;
}

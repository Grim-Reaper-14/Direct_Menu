#pragma once

#include <imgui.h>

#include <iosfwd>
#include <string_view>

namespace smf::core {

class ImGuiStyleSettings {
public:
    void Capture(const ImGuiStyle& style);
    void Apply(ImGuiStyle& style) const;
    void Reset() noexcept;

    [[nodiscard]] bool HasValue() const noexcept;
    bool SetValue(std::string_view key, std::string_view value);
    void Write(std::ostream& output) const;

private:
    ImGuiStyle style_{};
    bool hasValue_{false};
};

} // namespace smf::core

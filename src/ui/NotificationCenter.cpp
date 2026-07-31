#include "ui/NotificationCenter.hpp"

#include <imgui.h>

#include <algorithm>
#include <format>

namespace smf::ui {
namespace {

ImVec4 KindColor(const NotificationKind kind) {
    switch (kind) {
    case NotificationKind::Info:
        return {0.28F, 0.62F, 0.95F, 1.0F};
    case NotificationKind::Success:
        return {0.24F, 0.78F, 0.48F, 1.0F};
    case NotificationKind::Warning:
        return {0.95F, 0.68F, 0.22F, 1.0F};
    case NotificationKind::Error:
        return {0.92F, 0.30F, 0.34F, 1.0F};
    }
    return {0.75F, 0.75F, 0.75F, 1.0F};
}

} // namespace

void NotificationCenter::Push(
    std::string message,
    const NotificationKind kind,
    const double duration) {
    notifications_.push_back({
        std::move(message),
        kind,
        ImGui::GetTime(),
        std::max(duration, 0.5)
    });
}

void NotificationCenter::Render() {
    const double now = ImGui::GetTime();
    std::erase_if(
        notifications_,
        [now](const Notification& notification) {
            return now - notification.createdAt >= notification.duration;
        });

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float verticalOffset = 18.0F;

    for (std::size_t index = 0; index < notifications_.size(); ++index) {
        const Notification& notification =
            notifications_[notifications_.size() - 1 - index];

        const double age = now - notification.createdAt;
        const double remaining = notification.duration - age;
        float alpha = 1.0F;
        if (age < 0.2) {
            alpha = static_cast<float>(age / 0.2);
        } else if (remaining < 0.35) {
            alpha = static_cast<float>(remaining / 0.35);
        }

        ImGui::SetNextWindowBgAlpha(0.94F * std::clamp(alpha, 0.0F, 1.0F));
        ImGui::SetNextWindowPos(
            {
                viewport->WorkPos.x + viewport->WorkSize.x - 16.0F,
                viewport->WorkPos.y + viewport->WorkSize.y - verticalOffset
            },
            ImGuiCond_Always,
            {1.0F, 1.0F});

        const std::string windowId = std::format("##notification_{}", index);
        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12.0F, 9.0F});
        if (ImGui::Begin(windowId.c_str(), nullptr, flags)) {
            ImGui::PushStyleColor(ImGuiCol_Text, KindColor(notification.kind));
            ImGui::TextUnformatted(notification.message.c_str());
            ImGui::PopStyleColor();
            verticalOffset += ImGui::GetWindowSize().y + 8.0F;
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

} // namespace smf::ui


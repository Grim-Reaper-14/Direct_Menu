#pragma once

#include <string>
#include <vector>

namespace smf::ui {

enum class NotificationKind {
    Info,
    Success,
    Warning,
    Error
};

struct Notification {
    std::string message;
    NotificationKind kind{NotificationKind::Info};
    double createdAt{0.0};
    double duration{3.5};
};

class NotificationCenter {
public:
    void Push(
        std::string message,
        NotificationKind kind = NotificationKind::Info,
        double duration = 3.5);
    void Render();

private:
    std::vector<Notification> notifications_;
};

} // namespace smf::ui


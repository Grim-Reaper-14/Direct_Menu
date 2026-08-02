#pragma once

#include <string>

namespace smf::core {
class Logger;
}

namespace smf::providers {

class GameProvider final {
public:
    explicit GameProvider(core::Logger* logger = nullptr);

    bool SetInvincibility(bool enabled, std::string& errorMessage);
    [[nodiscard]] bool InvincibilityEnabled() const noexcept;

private:
    core::Logger* logger_{nullptr};
    bool invincibilityEnabled_{false};
};

} // namespace smf::providers

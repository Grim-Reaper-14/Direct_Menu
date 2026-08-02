#include "providers/GameProvider.hpp"

#include "core/Logger.hpp"

namespace smf::providers {

GameProvider::GameProvider(core::Logger* logger)
    : logger_(logger) {
}

bool GameProvider::SetInvincibility(
    const bool enabled,
    std::string& errorMessage) {
    // Authorized integration point:
    // Replace the failure below with your supported provider call. Update
    // invincibilityEnabled_ and return true only after that provider confirms
    // the requested state. The menu automatically rolls back on false.
    (void)enabled;
    errorMessage = "No authorized invincibility implementation is configured.";
    if (logger_ != nullptr) {
        logger_->Warning(errorMessage);
    }
    return false;
}

bool GameProvider::InvincibilityEnabled() const noexcept {
    return invincibilityEnabled_;
}

} // namespace smf::providers

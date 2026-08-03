#include "sdk/PlayerManager.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

PlayerManager::PlayerManager(SDK& services) noexcept
    : services_(&services) {
}

Player PlayerManager::FromHandle(const EntityHandle handle) const noexcept {
    return Player(*services_, handle);
}

bool PlayerManager::HasLocal() const noexcept {
    return localHandle_ != InvalidEntityHandle;
}

EntityHandle PlayerManager::LocalHandle() const noexcept {
    return localHandle_;
}

Player PlayerManager::Local() const noexcept {
    return FromHandle(localHandle_);
}

void PlayerManager::SetLocalHandle(const EntityHandle handle) noexcept {
    localHandle_ = handle;
}

void PlayerManager::ClearLocal() noexcept {
    localHandle_ = InvalidEntityHandle;
}

} // namespace smf::sdk

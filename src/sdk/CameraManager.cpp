#include "sdk/CameraManager.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

CameraManager::CameraManager(SDK& services) noexcept
    : services_(&services) {
}

Camera CameraManager::FromHandle(const CameraHandle handle) const noexcept {
    return Camera(*services_, handle);
}

} // namespace smf::sdk

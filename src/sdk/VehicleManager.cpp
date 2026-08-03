#include "sdk/VehicleManager.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

VehicleManager::VehicleManager(SDK& services) noexcept
    : services_(&services) {
}

Vehicle VehicleManager::FromHandle(const EntityHandle handle) const noexcept {
    return Vehicle(*services_, handle);
}

} // namespace smf::sdk

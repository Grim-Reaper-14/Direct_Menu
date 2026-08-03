#include "sdk/WorldObjectManager.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

WorldObjectManager::WorldObjectManager(SDK& services) noexcept
    : services_(&services) {
}

WorldObject WorldObjectManager::FromHandle(
    const EntityHandle handle) const noexcept {
    return WorldObject(*services_, handle);
}

} // namespace smf::sdk

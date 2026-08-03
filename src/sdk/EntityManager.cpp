#include "sdk/EntityManager.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

EntityManager::EntityManager(SDK& services) noexcept
    : services_(&services) {
}

Entity EntityManager::FromHandle(const EntityHandle handle) const noexcept {
    return Entity(*services_, handle);
}

} // namespace smf::sdk

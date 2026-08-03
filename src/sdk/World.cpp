#include "sdk/World.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

World::World(SDK& services) noexcept
    : services_(&services) {
}

PlayerManager& World::Players() noexcept {
    return services_->Players();
}

const PlayerManager& World::Players() const noexcept {
    return services_->Players();
}

VehicleManager& World::Vehicles() noexcept {
    return services_->Vehicles();
}

const VehicleManager& World::Vehicles() const noexcept {
    return services_->Vehicles();
}

CameraManager& World::Cameras() noexcept {
    return services_->Cameras();
}

const CameraManager& World::Cameras() const noexcept {
    return services_->Cameras();
}

} // namespace smf::sdk

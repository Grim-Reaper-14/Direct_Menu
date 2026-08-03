#include "sdk/Vehicle.hpp"

namespace smf::sdk {

Vehicle::Vehicle(SDK& services, const EntityHandle handle) noexcept
    : Entity(services, handle) {
}

} // namespace smf::sdk

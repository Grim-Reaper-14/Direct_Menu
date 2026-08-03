#include "sdk/WorldObject.hpp"

namespace smf::sdk {

WorldObject::WorldObject(SDK& services, const EntityHandle handle) noexcept
    : Entity(services, handle) {
}

} // namespace smf::sdk

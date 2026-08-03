#pragma once

#include "sdk/Entity.hpp"

namespace smf::sdk {

class WorldObject final : public Entity {
public:
    WorldObject() noexcept = default;
    WorldObject(SDK& services, EntityHandle handle) noexcept;
};

} // namespace smf::sdk

#pragma once

#include "sdk/Entity.hpp"

namespace smf::sdk {

class Player final : public Entity {
public:
    Player() noexcept = default;
    Player(SDK& services, EntityHandle handle) noexcept;
};

} // namespace smf::sdk

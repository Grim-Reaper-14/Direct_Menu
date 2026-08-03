#pragma once

#include "sdk/Entity.hpp"

namespace smf::sdk {

class Vehicle final : public Entity {
public:
    Vehicle() noexcept = default;
    Vehicle(SDK& services, EntityHandle handle) noexcept;
};

} // namespace smf::sdk

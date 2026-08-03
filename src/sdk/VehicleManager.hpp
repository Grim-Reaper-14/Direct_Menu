#pragma once

#include "sdk/Vehicle.hpp"

namespace smf::sdk {

class SDK;

class VehicleManager final {
public:
    explicit VehicleManager(SDK& services) noexcept;

    [[nodiscard]] Vehicle FromHandle(EntityHandle handle) const noexcept;

private:
    SDK* services_{};
};

} // namespace smf::sdk

#pragma once

#include "sdk/WorldObject.hpp"

namespace smf::sdk {

class SDK;

class WorldObjectManager final {
public:
    explicit WorldObjectManager(SDK& services) noexcept;

    [[nodiscard]] WorldObject FromHandle(EntityHandle handle) const noexcept;

private:
    SDK* services_{};
};

} // namespace smf::sdk

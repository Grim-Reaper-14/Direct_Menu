#pragma once

#include "sdk/Entity.hpp"

namespace smf::sdk {

class SDK;

class EntityManager final {
public:
    explicit EntityManager(SDK& services) noexcept;

    [[nodiscard]] Entity FromHandle(EntityHandle handle) const noexcept;

private:
    SDK* services_{};
};

} // namespace smf::sdk

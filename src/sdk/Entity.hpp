#pragma once

#include "sdk/Types.hpp"

namespace smf::sdk {

class SDK;

class Entity {
public:
    Entity() noexcept = default;
    Entity(SDK& services, EntityHandle handle) noexcept;

    [[nodiscard]] bool IsBound() const noexcept;
    [[nodiscard]] bool HasValue() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] EntityHandle Handle() const noexcept;

    [[nodiscard]] SDK* Services() noexcept;
    [[nodiscard]] const SDK* Services() const noexcept;

    void Reset() noexcept;

    friend bool operator==(const Entity&, const Entity&) noexcept = default;

private:
    SDK* services_{};
    EntityHandle handle_{InvalidEntityHandle};
};

} // namespace smf::sdk

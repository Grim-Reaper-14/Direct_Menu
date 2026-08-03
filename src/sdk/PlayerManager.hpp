#pragma once

#include "sdk/Player.hpp"

namespace smf::sdk {

class SDK;

class PlayerManager final {
public:
    explicit PlayerManager(SDK& services) noexcept;

    [[nodiscard]] Player FromHandle(EntityHandle handle) const noexcept;

    [[nodiscard]] bool HasLocal() const noexcept;
    [[nodiscard]] EntityHandle LocalHandle() const noexcept;
    [[nodiscard]] Player Local() const noexcept;

    void SetLocalHandle(EntityHandle handle) noexcept;
    void ClearLocal() noexcept;

private:
    SDK* services_{};
    EntityHandle localHandle_{InvalidEntityHandle};
};

} // namespace smf::sdk

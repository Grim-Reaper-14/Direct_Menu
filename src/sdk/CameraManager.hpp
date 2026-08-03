#pragma once

#include "sdk/Camera.hpp"

namespace smf::sdk {

class SDK;

class CameraManager final {
public:
    explicit CameraManager(SDK& services) noexcept;

    [[nodiscard]] Camera FromHandle(CameraHandle handle) const noexcept;

private:
    SDK* services_{};
};

} // namespace smf::sdk

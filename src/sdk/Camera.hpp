#pragma once

#include "sdk/Types.hpp"

namespace smf::sdk {

class SDK;

class Camera final {
public:
    Camera() noexcept = default;
    Camera(SDK& services, CameraHandle handle) noexcept;

    [[nodiscard]] bool IsBound() const noexcept;
    [[nodiscard]] bool HasValue() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] CameraHandle Handle() const noexcept;

    [[nodiscard]] SDK* Services() noexcept;
    [[nodiscard]] const SDK* Services() const noexcept;

    void Reset() noexcept;

    friend bool operator==(const Camera&, const Camera&) noexcept = default;

private:
    SDK* services_{};
    CameraHandle handle_{InvalidCameraHandle};
};

} // namespace smf::sdk

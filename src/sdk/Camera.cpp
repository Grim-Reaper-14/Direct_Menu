#include "sdk/Camera.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

Camera::Camera(SDK& services, const CameraHandle handle) noexcept
    : services_(&services), handle_(handle) {
}

bool Camera::IsBound() const noexcept {
    return services_ != nullptr;
}

bool Camera::HasValue() const noexcept {
    return IsBound() && handle_ != InvalidCameraHandle;
}

Camera::operator bool() const noexcept {
    return HasValue();
}

CameraHandle Camera::Handle() const noexcept {
    return handle_;
}

SDK* Camera::Services() noexcept {
    return services_;
}

const SDK* Camera::Services() const noexcept {
    return services_;
}

void Camera::Reset() noexcept {
    services_ = nullptr;
    handle_ = InvalidCameraHandle;
}

} // namespace smf::sdk

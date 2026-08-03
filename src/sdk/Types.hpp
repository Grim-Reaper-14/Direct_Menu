#pragma once

#include <cmath>
#include <cstdint>

namespace smf::sdk {

using EntityHandle = std::uint32_t;
inline constexpr EntityHandle InvalidEntityHandle{};

struct Vector3 {
    float x{};
    float y{};
    float z{};

    [[nodiscard]] constexpr Vector3 operator+(const Vector3& other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }

    [[nodiscard]] constexpr Vector3 operator-(const Vector3& other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }

    [[nodiscard]] constexpr Vector3 operator*(const float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }

    [[nodiscard]] constexpr float LengthSquared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] float Length() const noexcept {
        return std::sqrt(LengthSquared());
    }

    [[nodiscard]] float DistanceTo(const Vector3& other) const noexcept {
        return (*this - other).Length();
    }
};

struct Rotation {
    float pitch{};
    float roll{};
    float yaw{};
};

} // namespace smf::sdk

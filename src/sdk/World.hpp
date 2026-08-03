#pragma once

namespace smf::sdk {

class CameraManager;
class EntityManager;
class PlayerManager;
class SDK;
class VehicleManager;

class World final {
public:
    explicit World(SDK& services) noexcept;

    [[nodiscard]] EntityManager& Entities() noexcept;
    [[nodiscard]] const EntityManager& Entities() const noexcept;

    [[nodiscard]] PlayerManager& Players() noexcept;
    [[nodiscard]] const PlayerManager& Players() const noexcept;

    [[nodiscard]] VehicleManager& Vehicles() noexcept;
    [[nodiscard]] const VehicleManager& Vehicles() const noexcept;

    [[nodiscard]] CameraManager& Cameras() noexcept;
    [[nodiscard]] const CameraManager& Cameras() const noexcept;

private:
    SDK* services_{};
};

} // namespace smf::sdk

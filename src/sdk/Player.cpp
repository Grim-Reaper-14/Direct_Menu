#include "sdk/Player.hpp"

namespace smf::sdk {

Player::Player(SDK& services, const EntityHandle handle) noexcept
    : Entity(services, handle) {
}

} // namespace smf::sdk

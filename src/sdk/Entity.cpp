#include "sdk/Entity.hpp"

#include "sdk/SDK.hpp"

namespace smf::sdk {

Entity::Entity(SDK& services, const EntityHandle handle) noexcept
    : services_(&services), handle_(handle) {
}

bool Entity::IsBound() const noexcept {
    return services_ != nullptr;
}

bool Entity::HasValue() const noexcept {
    return IsBound() && handle_ != InvalidEntityHandle;
}

Entity::operator bool() const noexcept {
    return HasValue();
}

EntityHandle Entity::Handle() const noexcept {
    return handle_;
}

SDK* Entity::Services() noexcept {
    return services_;
}

const SDK* Entity::Services() const noexcept {
    return services_;
}

void Entity::Reset() noexcept {
    services_ = nullptr;
    handle_ = InvalidEntityHandle;
}

} // namespace smf::sdk

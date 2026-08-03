#pragma once

#include "natives/NativeInvoker.hpp"
#include "sdk/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace smf::sdk {

struct SDKDiagnostics {
    bool processAttached{};
    std::size_t signatureDefinitions{};

    bool nativeBackendAvailable{};
    natives::NativeInvoker::Environment nativeEnvironment{
        natives::NativeInvoker::Environment::Unknown};
    bool nativeInvocationAllowed{};
    std::size_t registeredNatives{};

    std::size_t pendingNativeTasks{};
    std::uint64_t schedulerFrames{};
    std::uint64_t executedNativeTasks{};
    std::uint64_t failedNativeTasks{};

    std::size_t crossmapEntries{};
    std::string crossmapVersion;
    std::size_t nativeMetadataEntries{};

    bool hasLocalPlayer{};
    EntityHandle localPlayerHandle{InvalidEntityHandle};

    [[nodiscard]] std::string_view EnvironmentName() const noexcept;
};

} // namespace smf::sdk

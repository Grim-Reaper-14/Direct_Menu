#include "sdk/SDKDiagnostics.hpp"

namespace smf::sdk {

std::string_view SDKDiagnostics::EnvironmentName() const noexcept {
    switch (nativeEnvironment) {
    case natives::NativeInvoker::Environment::Unknown:
        return "Unknown";
    case natives::NativeInvoker::Environment::SinglePlayer:
        return "Single Player";
    case natives::NativeInvoker::Environment::PrivateAuthorized:
        return "Private Authorized";
    case natives::NativeInvoker::Environment::PublicOnline:
        return "Public Online";
    }
    return "Unknown";
}

} // namespace smf::sdk

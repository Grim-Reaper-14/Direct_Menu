#include "natives/NativeInvoker.hpp"

#include <mutex>
#include <vector>

namespace smf::natives {
namespace {

std::mutex g_generatedRuntimeMutex;
NativeInvoker* g_generatedInvoker = nullptr;
std::vector<NativeHash> g_generatedHashes;
thread_local std::string g_generatedLastError;

} // namespace

void BindGeneratedNativeRuntime(
    NativeInvoker& invoker,
    const std::span<const NativeHash> hashes) noexcept {
    std::scoped_lock lock(g_generatedRuntimeMutex);
    g_generatedInvoker = &invoker;
    g_generatedHashes.assign(hashes.begin(), hashes.end());
}

void UnbindGeneratedNativeRuntime() noexcept {
    std::scoped_lock lock(g_generatedRuntimeMutex);
    g_generatedInvoker = nullptr;
    g_generatedHashes.clear();
}

NativeInvoker* GeneratedNativeInvoker() noexcept {
    std::scoped_lock lock(g_generatedRuntimeMutex);
    return g_generatedInvoker;
}

NativeHash GeneratedNativeHash(const std::size_t index) noexcept {
    std::scoped_lock lock(g_generatedRuntimeMutex);
    if (index >= g_generatedHashes.size()) {
        return 0;
    }
    return g_generatedHashes[index];
}

const std::string& GeneratedNativeLastError() noexcept {
    return g_generatedLastError;
}

void SetGeneratedNativeLastError(std::string errorMessage) {
    g_generatedLastError = std::move(errorMessage);
}

} // namespace smf::natives

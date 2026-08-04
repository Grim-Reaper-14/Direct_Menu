#pragma once

#include <utility>

namespace smf::natives {
namespace generated_runtime_detail {

inline std::mutex mutex;
inline NativeInvoker* invoker = nullptr;
inline std::vector<NativeHash> hashes;
inline thread_local std::string lastError;

} // namespace generated_runtime_detail

inline void BindGeneratedNativeRuntime(
    NativeInvoker& nativeInvoker,
    const std::span<const NativeHash> nativeHashes) noexcept {
    std::scoped_lock lock(generated_runtime_detail::mutex);
    generated_runtime_detail::invoker = &nativeInvoker;
    generated_runtime_detail::hashes.assign(
        nativeHashes.begin(),
        nativeHashes.end());
}

inline void UnbindGeneratedNativeRuntime() noexcept {
    std::scoped_lock lock(generated_runtime_detail::mutex);
    generated_runtime_detail::invoker = nullptr;
    generated_runtime_detail::hashes.clear();
}

inline NativeInvoker* GeneratedNativeInvoker() noexcept {
    std::scoped_lock lock(generated_runtime_detail::mutex);
    return generated_runtime_detail::invoker;
}

inline NativeHash GeneratedNativeHash(const std::size_t index) noexcept {
    std::scoped_lock lock(generated_runtime_detail::mutex);
    if (index >= generated_runtime_detail::hashes.size()) {
        return 0;
    }
    return generated_runtime_detail::hashes[index];
}

inline const std::string& GeneratedNativeLastError() noexcept {
    return generated_runtime_detail::lastError;
}

inline void SetGeneratedNativeLastError(std::string errorMessage) {
    generated_runtime_detail::lastError = std::move(errorMessage);
}

} // namespace smf::natives

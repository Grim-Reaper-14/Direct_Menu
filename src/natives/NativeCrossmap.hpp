#pragma once

#include "natives/NativeInvoker.hpp"

#include <cstddef>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace smf::natives {

class NativeCrossmap final {
public:
    using Mapping = std::pair<NativeHash, NativeHash>;

    bool SetVersion(std::string version);
    [[nodiscard]] std::string Version() const;

    bool Register(NativeHash originalHash, NativeHash currentHash);
    std::size_t RegisterMany(
        std::span<const Mapping> mappings,
        bool replaceExisting = true);

    bool Remove(NativeHash originalHash);
    void Clear() noexcept;

    [[nodiscard]] bool Contains(NativeHash originalHash) const;
    [[nodiscard]] std::optional<NativeHash> Resolve(
        NativeHash originalHash) const;
    [[nodiscard]] NativeHash ResolveOrOriginal(
        NativeHash originalHash) const noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] std::vector<Mapping> Snapshot() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<NativeHash, NativeHash> mappings_;
    std::string version_;
};

} // namespace smf::natives

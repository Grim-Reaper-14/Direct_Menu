#pragma once

#include "natives/NativeCallContext.hpp"

#include <cstddef>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace smf::natives {

class NativeRegistry final {
public:
    using Entry = std::pair<NativeHash, NativeHandler>;

    bool Register(NativeHash hash, NativeHandler handler);
    bool Remove(NativeHash hash);
    void Clear() noexcept;

    [[nodiscard]] bool Contains(NativeHash hash) const;
    [[nodiscard]] std::optional<NativeHandler> Find(NativeHash hash) const;
    [[nodiscard]] std::size_t Size() const noexcept;

    std::size_t Cache(std::span<const Entry> entries, bool replaceExisting = true);

    [[nodiscard]] bool Invoke(
        NativeHash hash,
        NativeCallContext& context) const noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<NativeHash, NativeHandler> handlers_;
};

} // namespace smf::natives

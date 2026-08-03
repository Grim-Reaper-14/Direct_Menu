#pragma once

#include "core/MemoryManagerAPI.hpp"
#include "core/SignatureManager.hpp"
#include "natives/NativeCrossmap.hpp"
#include "natives/NativeDatabase.hpp"
#include "natives/NativeInvoker.hpp"
#include "natives/NativeRegistry.hpp"
#include "natives/NativeScheduler.hpp"

namespace smf::sdk {

class SDK final {
public:
    SDK(
        core::MemoryManagerAPI& memory,
        core::SignatureManager& signatures,
        natives::NativeInvoker& nativeInvoker,
        natives::NativeRegistry& nativeRegistry,
        natives::NativeScheduler& nativeScheduler,
        natives::NativeCrossmap& nativeCrossmap,
        natives::NativeDatabase& nativeDatabase) noexcept;

    [[nodiscard]] core::MemoryManagerAPI& Memory() noexcept;
    [[nodiscard]] const core::MemoryManagerAPI& Memory() const noexcept;

    [[nodiscard]] core::SignatureManager& Signatures() noexcept;
    [[nodiscard]] const core::SignatureManager& Signatures() const noexcept;

    [[nodiscard]] natives::NativeInvoker& NativeInvoker() noexcept;
    [[nodiscard]] const natives::NativeInvoker& NativeInvoker() const noexcept;

    [[nodiscard]] natives::NativeRegistry& NativeRegistry() noexcept;
    [[nodiscard]] const natives::NativeRegistry& NativeRegistry() const noexcept;

    [[nodiscard]] natives::NativeScheduler& NativeScheduler() noexcept;
    [[nodiscard]] const natives::NativeScheduler& NativeScheduler() const noexcept;

    [[nodiscard]] natives::NativeCrossmap& NativeCrossmap() noexcept;
    [[nodiscard]] const natives::NativeCrossmap& NativeCrossmap() const noexcept;

    [[nodiscard]] natives::NativeDatabase& NativeDatabase() noexcept;
    [[nodiscard]] const natives::NativeDatabase& NativeDatabase() const noexcept;

private:
    core::MemoryManagerAPI* memory_{};
    core::SignatureManager* signatures_{};
    natives::NativeInvoker* nativeInvoker_{};
    natives::NativeRegistry* nativeRegistry_{};
    natives::NativeScheduler* nativeScheduler_{};
    natives::NativeCrossmap* nativeCrossmap_{};
    natives::NativeDatabase* nativeDatabase_{};
};

} // namespace smf::sdk

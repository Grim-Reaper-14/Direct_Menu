#pragma once

#include "natives/NativeQueue.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace smf::natives {

class NativeScheduler final {
public:
    struct Statistics {
        std::uint64_t frames{};
        std::uint64_t executedTasks{};
        std::uint64_t failedTasks{};
        std::size_t lastFrameTasks{};
        std::chrono::nanoseconds lastFrameDuration{};
        std::chrono::nanoseconds totalDuration{};
    };

    NativeScheduler();

    bool Submit(NativeQueue::Task task);
    std::size_t Tick(std::size_t maximumTasks = 0) noexcept;
    void Clear() noexcept;

    void SetErrorHandler(NativeQueue::ErrorHandler handler);

    [[nodiscard]] std::size_t PendingCount() const noexcept;
    [[nodiscard]] Statistics Stats() const noexcept;
    void ResetStatistics() noexcept;

private:
    NativeQueue queue_;
    mutable std::mutex statisticsMutex_;
    Statistics statistics_{};
};

} // namespace smf::natives

#include "natives/NativeScheduler.hpp"

#include <utility>

namespace smf::natives {

NativeScheduler::NativeScheduler() {
    queue_.SetErrorHandler([this](const std::string&) {
        std::scoped_lock lock(statisticsMutex_);
        ++statistics_.failedTasks;
    });
}

bool NativeScheduler::Submit(NativeQueue::Task task) {
    return queue_.Submit(std::move(task));
}

std::size_t NativeScheduler::Tick(const std::size_t maximumTasks) noexcept {
    const auto startedAt = std::chrono::steady_clock::now();
    const std::size_t executed = queue_.ExecutePending(maximumTasks);
    const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - startedAt);

    {
        std::scoped_lock lock(statisticsMutex_);
        ++statistics_.frames;
        statistics_.executedTasks += executed;
        statistics_.lastFrameTasks = executed;
        statistics_.lastFrameDuration = duration;
        statistics_.totalDuration += duration;
    }

    return executed;
}

void NativeScheduler::Clear() noexcept {
    queue_.Clear();
}

void NativeScheduler::SetErrorHandler(NativeQueue::ErrorHandler handler) {
    queue_.SetErrorHandler(
        [this, handler = std::move(handler)](const std::string& message) {
            {
                std::scoped_lock lock(statisticsMutex_);
                ++statistics_.failedTasks;
            }

            if (handler) {
                handler(message);
            }
        });
}

std::size_t NativeScheduler::PendingCount() const noexcept {
    return queue_.PendingCount();
}

NativeScheduler::Statistics NativeScheduler::Stats() const noexcept {
    std::scoped_lock lock(statisticsMutex_);
    return statistics_;
}

void NativeScheduler::ResetStatistics() noexcept {
    std::scoped_lock lock(statisticsMutex_);
    statistics_ = {};
}

} // namespace smf::natives

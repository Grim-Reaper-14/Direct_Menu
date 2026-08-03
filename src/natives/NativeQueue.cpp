#include "natives/NativeQueue.hpp"

#include <exception>
#include <utility>

namespace smf::natives {

bool NativeQueue::Submit(Task task) {
    if (!task) {
        return false;
    }

    std::scoped_lock lock(mutex_);
    tasks_.push_back(std::move(task));
    return true;
}

std::size_t NativeQueue::ExecutePending(const std::size_t maximumTasks) noexcept {
    std::size_t executed = 0;

    while (maximumTasks == 0 || executed < maximumTasks) {
        Task task;
        ErrorHandler errorHandler;

        {
            std::scoped_lock lock(mutex_);
            if (tasks_.empty()) {
                break;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
            errorHandler = errorHandler_;
        }

        try {
            task();
        } catch (const std::exception& exception) {
            if (errorHandler) {
                try {
                    errorHandler(exception.what());
                } catch (...) {
                }
            }
        } catch (...) {
            if (errorHandler) {
                try {
                    errorHandler("Native queue task threw an unknown exception.");
                } catch (...) {
                }
            }
        }

        ++executed;
    }

    return executed;
}

void NativeQueue::Clear() noexcept {
    std::scoped_lock lock(mutex_);
    tasks_.clear();
}

void NativeQueue::SetErrorHandler(ErrorHandler handler) {
    std::scoped_lock lock(mutex_);
    errorHandler_ = std::move(handler);
}

std::size_t NativeQueue::PendingCount() const noexcept {
    std::scoped_lock lock(mutex_);
    return tasks_.size();
}

bool NativeQueue::Empty() const noexcept {
    std::scoped_lock lock(mutex_);
    return tasks_.empty();
}

} // namespace smf::natives

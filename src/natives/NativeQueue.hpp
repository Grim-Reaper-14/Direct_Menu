#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace smf::natives {

class NativeQueue final {
public:
    using Task = std::function<void()>;
    using ErrorHandler = std::function<void(const std::string&)>;

    bool Submit(Task task);
    std::size_t ExecutePending(std::size_t maximumTasks = 0) noexcept;
    void Clear() noexcept;

    void SetErrorHandler(ErrorHandler handler);

    [[nodiscard]] std::size_t PendingCount() const noexcept;
    [[nodiscard]] bool Empty() const noexcept;

private:
    mutable std::mutex mutex_;
    std::deque<Task> tasks_;
    ErrorHandler errorHandler_;
};

} // namespace smf::natives

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace smf::core {

class TaskQueue {
public:
    using Task = std::function<void()>;
    using ErrorHandler = std::function<void(std::string_view)>;

    TaskQueue() = default;
    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    void Start(std::size_t workerCount = 0);
    bool Enqueue(Task task);
    void SetErrorHandler(ErrorHandler handler);
    void Shutdown();

    [[nodiscard]] std::size_t Pending() const;
    [[nodiscard]] std::size_t WorkerCount() const;
    [[nodiscard]] bool Running() const;

private:
    void WorkerLoop(std::stop_token stopToken);
    void ReportError(std::string message) const;

    mutable std::mutex mutex_;
    std::condition_variable_any wakeCondition_;
    std::deque<Task> tasks_;
    std::vector<std::jthread> workers_;
    ErrorHandler errorHandler_;
    bool stopping_{false};
};

} // namespace smf::core


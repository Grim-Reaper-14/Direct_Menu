#include "core/TaskQueue.hpp"

#include <algorithm>
#include <exception>

namespace smf::core {

TaskQueue::~TaskQueue() {
    Shutdown();
}

void TaskQueue::Start(std::size_t workerCount) {
    std::scoped_lock lock{mutex_};
    if (!workers_.empty()) {
        return;
    }

    if (workerCount == 0) {
        const std::size_t hardwareThreads =
            static_cast<std::size_t>(std::thread::hardware_concurrency());
        workerCount = std::clamp<std::size_t>(
            hardwareThreads > 1 ? hardwareThreads / 2 : 1,
            1,
            4);
    }

    stopping_ = false;
    workers_.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index) {
        workers_.emplace_back(
            [this](const std::stop_token stopToken) {
                WorkerLoop(stopToken);
            });
    }
}

bool TaskQueue::Enqueue(Task task) {
    if (!task) {
        return false;
    }

    {
        std::scoped_lock lock{mutex_};
        if (stopping_ || workers_.empty()) {
            return false;
        }
        tasks_.push_back(std::move(task));
    }

    wakeCondition_.notify_one();
    return true;
}

void TaskQueue::SetErrorHandler(ErrorHandler handler) {
    std::scoped_lock lock{mutex_};
    errorHandler_ = std::move(handler);
}

void TaskQueue::Shutdown() {
    std::vector<std::jthread> workersToJoin;

    {
        std::scoped_lock lock{mutex_};
        if (workers_.empty()) {
            tasks_.clear();
            stopping_ = true;
            return;
        }
        stopping_ = true;
        for (auto& worker : workers_) {
            worker.request_stop();
        }
        workersToJoin.swap(workers_);
    }

    wakeCondition_.notify_all();
    workersToJoin.clear();

    std::scoped_lock lock{mutex_};
    tasks_.clear();
}

std::size_t TaskQueue::Pending() const {
    std::scoped_lock lock{mutex_};
    return tasks_.size();
}

std::size_t TaskQueue::WorkerCount() const {
    std::scoped_lock lock{mutex_};
    return workers_.size();
}

bool TaskQueue::Running() const {
    std::scoped_lock lock{mutex_};
    return !workers_.empty() && !stopping_;
}

void TaskQueue::WorkerLoop(const std::stop_token stopToken) {
    while (true) {
        Task task;

        {
            std::unique_lock lock{mutex_};
            wakeCondition_.wait(
                lock,
                stopToken,
                [this] {
                    return stopping_ || !tasks_.empty();
                });

            if (tasks_.empty()) {
                if (stopping_ || stopToken.stop_requested()) {
                    return;
                }
                continue;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        try {
            task();
        } catch (const std::exception& exception) {
            ReportError(exception.what());
        } catch (...) {
            ReportError("A background task threw an unknown exception.");
        }
    }
}

void TaskQueue::ReportError(std::string message) const {
    ErrorHandler handler;
    {
        std::scoped_lock lock{mutex_};
        handler = errorHandler_;
    }
    if (handler) {
        handler(message);
    }
}

} // namespace smf::core

#include "core/TaskQueue.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <future>

int main() {
    smf::core::TaskQueue queue;
    queue.Start(2);
    assert(queue.Running());
    assert(queue.WorkerCount() == 2);

    std::promise<void> completed;
    std::future<void> completion = completed.get_future();
    std::atomic<int> total{0};

    assert(queue.Enqueue([&total] {
        total.fetch_add(2);
    }));
    assert(queue.Enqueue([&total, &completed] {
        total.fetch_add(3);
        completed.set_value();
    }));

    assert(
        completion.wait_for(std::chrono::seconds{2}) ==
        std::future_status::ready);
    queue.Shutdown();
    assert(!queue.Running());
    assert(total.load() == 5);
    assert(!queue.Enqueue([] {}));
    return 0;
}


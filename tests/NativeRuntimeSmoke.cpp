#include "natives/NativeCallContext.hpp"
#include "natives/NativeInvoker.hpp"
#include "natives/NativeQueue.hpp"
#include "natives/NativeRegistry.hpp"
#include "natives/NativeScheduler.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

void AddHandler(smf::natives::NativeCallContext& context) {
    const auto arguments = context.Arguments();
    if (arguments.size() == 2) {
        context.ReturnStorage()[0] = arguments[0] + arguments[1];
    }
}

} // namespace

int main() {
    using namespace smf::natives;

    NativeRegistry registry;
    constexpr NativeHash AddHash = 0x1001;
    if (!registry.Register(AddHash, &AddHandler) || registry.Size() != 1) {
        return Fail("NativeRegistry registration failed.");
    }

    NativeCallContext context;
    context.Push<std::uint64_t>(20);
    context.Push<std::uint64_t>(22);
    if (!registry.Invoke(AddHash, context) ||
        context.Result<std::uint64_t>() != 42) {
        return Fail("NativeRegistry invocation failed.");
    }

    NativeInvoker invoker;
    invoker.SetEnvironmentProbe([] {
        return NativeInvoker::Environment::SinglePlayer;
    });
    invoker.SetBackend([&registry](
                           const NativeHash hash,
                           const std::span<const NativeWord> arguments,
                           NativeWord& result,
                           std::string& error) {
        NativeCallContext backendContext;
        for (const NativeWord argument : arguments) {
            backendContext.Push(argument);
        }
        if (!registry.Invoke(hash, backendContext)) {
            error = "Handler not found.";
            return false;
        }
        result = backendContext.ReturnStorage()[0];
        return true;
    });

    std::string error;
    const auto sum = invoker.Invoke<std::uint64_t>(AddHash, error, 7ULL, 8ULL);
    if (!sum || *sum != 15 || !error.empty()) {
        return Fail("NativeInvoker typed invocation failed.");
    }

    invoker.SetEnvironmentProbe([] {
        return NativeInvoker::Environment::PublicOnline;
    });
    if (invoker.Invoke<std::uint64_t>(AddHash, error, 1ULL, 2ULL)) {
        return Fail("NativeInvoker did not fail closed in PublicOnline.");
    }

    NativeScheduler scheduler;
    std::vector<int> order;
    std::size_t reportedErrors = 0;
    scheduler.SetErrorHandler([&reportedErrors](const std::string&) {
        ++reportedErrors;
    });

    scheduler.Submit([&order] { order.push_back(1); });
    scheduler.Submit([&order] { order.push_back(2); });
    scheduler.Submit([] { throw std::runtime_error("expected smoke failure"); });
    scheduler.Submit([&order] { order.push_back(3); });

    if (scheduler.Tick(2) != 2 || order != std::vector<int>({1, 2}) ||
        scheduler.PendingCount() != 2) {
        return Fail("NativeScheduler task limit or FIFO order failed.");
    }

    if (scheduler.Tick() != 2 || order != std::vector<int>({1, 2, 3}) ||
        reportedErrors != 1 || scheduler.PendingCount() != 0) {
        return Fail("NativeScheduler draining or error isolation failed.");
    }

    const auto statistics = scheduler.Stats();
    if (statistics.frames != 2 || statistics.executedTasks != 4 ||
        statistics.failedTasks != 1 || statistics.lastFrameTasks != 2) {
        return Fail("NativeScheduler statistics failed.");
    }

    return 0;
}

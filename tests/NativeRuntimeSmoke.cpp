#include "natives/NativeCallContext.hpp"
#include "natives/NativeCrossmap.hpp"
#include "natives/NativeHookManager.hpp"
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

    // -----------------------------------------------------------------------
    // NativeHookManager: basic hook + invocation through NativeInvoker
    // -----------------------------------------------------------------------
    {
        auto hookRegistry = std::make_shared<NativeRegistry>();
        auto hookCrossmap = std::make_shared<NativeCrossmap>();

        NativeHookManager hookManager(hookRegistry, hookCrossmap);

        NativeInvoker hookInvoker;
        hookInvoker.SetEnvironmentProbe([] {
            return NativeInvoker::Environment::SinglePlayer;
        });

        hookManager.Attach(hookInvoker);
        if (!hookManager.IsAttached()) {
            return Fail("NativeHookManager Attach failed.");
        }

        constexpr NativeHash MultiplyHash = 0x2001;

        const bool hooked = hookManager.Hook(
            MultiplyHash,
            [](NativeCallContext& ctx) {
                const auto args = ctx.Arguments();
                if (args.size() == 2) {
                    ctx.ReturnStorage()[0] = args[0] * args[1];
                }
            });
        if (!hooked || hookManager.HookCount() != 1) {
            return Fail("NativeHookManager Hook registration failed.");
        }

        std::string hookError;
        const auto product = hookInvoker.Invoke<std::uint64_t>(
            MultiplyHash, hookError, 6ULL, 7ULL);
        if (!product || *product != 42 || !hookError.empty()) {
            return Fail("NativeHookManager hook invocation failed.");
        }

        // Unhook and verify failure with a clear error message.
        if (!hookManager.Unhook(MultiplyHash)) {
            return Fail("NativeHookManager Unhook failed.");
        }
        if (hookManager.HookCount() != 0) {
            return Fail("NativeHookManager HookCount after unhook failed.");
        }

        const auto afterUnhook = hookInvoker.Invoke<std::uint64_t>(
            MultiplyHash, hookError, 6ULL, 7ULL);
        if (afterUnhook || hookError.empty()) {
            return Fail("NativeHookManager should fail after unhook.");
        }
    }

    // -----------------------------------------------------------------------
    // NativeHookManager: crossmap hash translation
    // -----------------------------------------------------------------------
    {
        auto hookRegistry = std::make_shared<NativeRegistry>();
        auto hookCrossmap = std::make_shared<NativeCrossmap>();

        constexpr NativeHash OriginalHash = 0x3001;
        constexpr NativeHash CurrentHash  = 0x3002;

        // Register a crossmap entry: original → current
        hookCrossmap->Register(OriginalHash, CurrentHash);

        NativeHookManager hookManager(hookRegistry, hookCrossmap);

        NativeInvoker hookInvoker;
        hookInvoker.SetEnvironmentProbe([] {
            return NativeInvoker::Environment::SinglePlayer;
        });
        hookManager.Attach(hookInvoker);

        // Hook using the original hash; internally it should be stored under
        // the current hash after crossmap resolution.
        hookManager.Hook(
            OriginalHash,
            [](NativeCallContext& ctx) {
                ctx.ReturnStorage()[0] = 99;
            });

        std::string crossmapError;
        // Invoke with the original hash; crossmap should translate it.
        const auto crossmapResult = hookInvoker.Invoke<std::uint64_t>(
            OriginalHash, crossmapError);
        if (!crossmapResult || *crossmapResult != 99 || !crossmapError.empty()) {
            return Fail("NativeHookManager crossmap translation failed.");
        }
    }

    // -----------------------------------------------------------------------
    // NativeHookManager: fallback backend when no hook is registered
    // -----------------------------------------------------------------------
    {
        auto hookRegistry = std::make_shared<NativeRegistry>();
        auto hookCrossmap = std::make_shared<NativeCrossmap>();

        NativeHookManager hookManager(hookRegistry, hookCrossmap);

        NativeInvoker hookInvoker;
        hookInvoker.SetEnvironmentProbe([] {
            return NativeInvoker::Environment::SinglePlayer;
        });
        hookManager.Attach(hookInvoker);

        constexpr NativeHash FallbackHash = 0x4001;
        bool fallbackCalled = false;

        hookManager.SetFallbackBackend(
            [&fallbackCalled](
                NativeHash /*hash*/,
                std::span<const NativeWord> /*args*/,
                NativeWord& result,
                std::string& /*err*/) {
                fallbackCalled = true;
                result = 77;
                return true;
            });

        std::string fallbackError;
        const auto fallbackResult =
            hookInvoker.Invoke<std::uint64_t>(FallbackHash, fallbackError);
        if (!fallbackResult || *fallbackResult != 77 || !fallbackCalled ||
            !fallbackError.empty()) {
            return Fail("NativeHookManager fallback backend failed.");
        }
    }

    // -----------------------------------------------------------------------
    // NativeHookManager: Detach removes backend from invoker
    // -----------------------------------------------------------------------
    {
        auto hookRegistry = std::make_shared<NativeRegistry>();
        auto hookCrossmap = std::make_shared<NativeCrossmap>();

        NativeHookManager hookManager(hookRegistry, hookCrossmap);

        NativeInvoker hookInvoker;
        hookInvoker.SetEnvironmentProbe([] {
            return NativeInvoker::Environment::SinglePlayer;
        });

        hookManager.Attach(hookInvoker);
        hookManager.Detach();

        if (hookManager.IsAttached()) {
            return Fail("NativeHookManager IsAttached after Detach failed.");
        }
        if (hookInvoker.HasBackend()) {
            return Fail("NativeHookManager Detach should clear invoker backend.");
        }
    }

    return 0;
}

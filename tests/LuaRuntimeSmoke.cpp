#include "core/Logger.hpp"
#include "core/MemoryManagerAPI.hpp"
#include "core/SignatureManager.hpp"
#include "features/FeatureRegistry.hpp"
#include "logging/Logger.hpp"
#include "natives/NativeCrossmap.hpp"
#include "natives/NativeDatabase.hpp"
#include "natives/NativeInvoker.hpp"
#include "natives/NativeRegistry.hpp"
#include "natives/NativeScheduler.hpp"
#include "scripting/LuaManager.hpp"
#include "sdk/SDK.hpp"

#include <imgui.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int Fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path root =
        std::filesystem::current_path() /
        ("Direct_Menu_LuaRuntimeSmoke_" + unique);
    const std::filesystem::path scripts = root / "LuaScripts";
    const std::filesystem::path scriptPath = scripts / "runtime_smoke.lua";
    const std::filesystem::path logPath = root / "latest.log";

    std::error_code filesystemError;
    std::filesystem::create_directories(scripts, filesystemError);
    if (filesystemError) {
        return Fail("Could not create the Lua runtime smoke-test directory.");
    }

    {
        std::ofstream script{scriptPath, std::ios::out | std::ios::trunc};
        script << R"lua(
direct.plugin({
    author = "Smoke Test",
    version = "1.0.0",
    api = "2.1",
    description = "Lua runtime integration smoke test",
    permissions = { "ui", "events", "filesystem.write" }
})
direct.log.info("Lua runtime smoke script loaded")
direct.ui.text("Lua runtime smoke UI")
direct.ui.button("Lua smoke callback", function()
    direct.log.info("Lua retained UI callback executed")
end)
local first_tick
first_tick = direct.events.on("tick", function()
    direct.log.info("Lua runtime smoke tick callback executed")
    local wrote = direct.files.write_text("callback-ok.txt", "callback executed\n")
    direct.log.info("Lua runtime smoke write result = " .. tostring(wrote))
    direct.events.off(first_tick)
end)
)lua";
    }

    smf::logging::LoggerApi logger;
    logger.SetConsoleOutputEnabled(true);
    if (!logger.Initialize(logPath)) {
        return Fail("Could not initialize the Lua runtime smoke-test logger.");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    int result = 0;
    {
        smf::core::Logger memoryLogger{logger, "MemorySmoke"};
        smf::core::MemoryManagerAPI memory{&memoryLogger};
        smf::core::SignatureManager signatures{memory};
        smf::natives::NativeInvoker nativeInvoker;
        smf::natives::NativeRegistry nativeRegistry;
        smf::natives::NativeScheduler nativeScheduler;
        smf::natives::NativeCrossmap nativeCrossmap;
        smf::natives::NativeDatabase nativeDatabase;
        smf::sdk::SDK services(
            memory,
            signatures,
            nativeInvoker,
            nativeRegistry,
            nativeScheduler,
            nativeCrossmap,
            nativeDatabase);
        services.Players().SetLocalHandle(77);
        smf::features::FeatureRegistry features;
        smf::scripting::LuaManager lua{logger};
        lua.BindFeatureRegistry(features);
        lua.BindMemoryAPI(memory, signatures);
        lua.BindSDK(services);
        lua.Initialize(scripts);

        const sol::protected_function_result bindingCheck = lua.State().safe_script(R"lua(
            assert(type(Memory) == "table")
            assert(type(Signatures) == "table")
            assert(Memory.is_attached() == false)
            assert(Memory.process_id() == 0)
            assert(Signatures.count() == 0)
            assert(Signatures.cached("missing") == nil)
            assert(type(SDK) == "table")
            local entity = SDK.entity(12)
            assert(entity:is_bound())
            assert(entity:valid())
            assert(entity:handle() == 12)
            assert(SDK.player(13):handle() == 13)
            assert(SDK.vehicle(14):handle() == 14)
            assert(SDK.camera(15):handle() == 15)
            assert(SDK.has_local_player())
            assert(SDK.local_player():handle() == 77)
            local status = SDK.status()
            assert(type(status) == "table")
            assert(status.process_attached == false)
            assert(status.native_backend == false)
            assert(status.environment == "Unknown")
            assert(status.invocation_allowed == false)
            assert(status.has_local_player == true)
            assert(status.local_player_handle == 77)
        )lua", sol::script_pass_on_error);

        if (!bindingCheck.valid()) {
            const sol::error error = bindingCheck;
            result = Fail(
                std::string{"The Lua Memory/Signatures binding smoke check failed: "} +
                error.what());
        } else if (!lua.RuntimeReady()) {
            result = Fail("Lua 5.4 + sol2 API v2 did not become ready.");
        } else if (lua.Scripts().size() != 1 || !lua.Scripts().front().loaded) {
            result = Fail("The runtime smoke script did not load.");
        } else if (lua.Scripts().front().author != "Smoke Test" ||
                   lua.Scripts().front().apiVersion != "2.1") {
            result = Fail("Lua manifest metadata was not recorded.");
        } else if (!lua.HasMenuContent()) {
            result = Fail("Lua-created UI content was not registered.");
        } else {
            lua.Update();
            const std::filesystem::path callbackMarker =
                lua.FileSystemSandbox().ScriptRoot("runtime_smoke.lua") /
                "callback-ok.txt";
            if (!std::filesystem::exists(callbackMarker)) {
                result = Fail(
                    "The Lua callback or permission-gated filesystem write failed.");
            } else if (!lua.FileSystemSandbox().WriteText(
                           "runtime_smoke.lua",
                           "callback-ok.txt",
                           "callback rewrite executed\n")) {
                result = Fail(
                    "The Lua filesystem sandbox could not rewrite an existing file.");
            } else if (!lua.ScriptsManager().Reload("runtime_smoke.lua") ||
                       !lua.Scripts().front().loaded ||
                       !lua.HasMenuContent()) {
                result = Fail("Lua script reload did not restore its UI content.");
            } else if (!lua.ScriptsManager().Unload("runtime_smoke.lua") ||
                       lua.Scripts().front().loaded ||
                       lua.HasMenuContent()) {
                result = Fail("Lua script unload did not clean up its UI content.");
            } else if (!lua.ScriptsManager().Load("runtime_smoke.lua") ||
                       !lua.Scripts().front().loaded ||
                       !lua.HasMenuContent()) {
                result = Fail("Lua script load did not restore its UI content.");
            }
        }

        lua.Shutdown();
    }

    ImGui::DestroyContext();
    logger.Shutdown();
    if (result == 0) {
        std::filesystem::remove_all(root, filesystemError);
    } else {
        std::cerr << "Smoke-test files retained at: " << root.string() << '\n';
    }
    return result;
}

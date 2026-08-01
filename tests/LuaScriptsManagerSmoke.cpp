#include "logging/Logger.hpp"
#include "scripting/LuaScriptsManager.hpp"

#include <sol/sol.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool Expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int main() {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path scriptsDirectory =
        std::filesystem::temp_directory_path() /
        ("Direct_Menu_LuaScriptsManagerSmoke_" + std::to_string(unique));

    std::error_code fileError;
    std::filesystem::create_directories(scriptsDirectory, fileError);
    if (!Expect(!fileError, "Could not create the Lua smoke-test directory.")) {
        return 1;
    }

    {
        std::ofstream script{scriptsDirectory / "metadata.lua"};
        script
            << "SCRIPT_METADATA = {\n"
            << "  author = 'Lua Author',\n"
            << "  version = '1.2.3',\n"
            << "  description = 'Smoke test script'\n"
            << "}\n"
            << "function on_unload() end\n";
    }
    {
        std::ofstream script{scriptsDirectory / "broken.lua"};
        script << "error('expected smoke-test failure')\n";
    }

    smf::logging::LoggerApi logger;
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    std::size_t cleanupCount = 0;
    smf::scripting::LuaScriptsManager scripts{
        logger,
        lua,
        [](std::string_view) {},
        [&cleanupCount](std::string_view) { ++cleanupCount; }};
    scripts.Initialize(scriptsDirectory);

    bool passed = true;
    passed &= Expect(scripts.Scripts().size() == 2, "Lua discovery count was incorrect.");
    passed &= Expect(scripts.Load("metadata.lua"), "Valid Lua script failed to load.");

    const smf::scripting::ScriptRecord* metadata = scripts.Find("metadata.lua");
    passed &= Expect(metadata != nullptr && metadata->loaded, "Loaded status was not recorded.");
    passed &= Expect(metadata != nullptr && metadata->author == "Lua Author", "Author metadata was not read.");
    passed &= Expect(metadata != nullptr && metadata->version == "1.2.3", "Version metadata was not read.");
    passed &= Expect(
        metadata != nullptr && metadata->description == "Smoke test script",
        "Description metadata was not read.");
    passed &= Expect(scripts.ReloadAll() == 1, "Reload All did not reload the active script.");
    passed &= Expect(scripts.Unload("metadata.lua"), "Valid Lua script failed to unload.");
    passed &= Expect(cleanupCount >= 2, "Owned-resource cleanup was not invoked.");

    passed &= Expect(!scripts.Load("broken.lua"), "Broken Lua script unexpectedly loaded.");
    const smf::scripting::ScriptRecord* broken = scripts.Find("broken.lua");
    passed &= Expect(
        broken != nullptr && broken->lastError.find("expected smoke-test failure") != std::string::npos,
        "Protected Lua error text was not retained.");

    scripts.UnloadAll();
    std::filesystem::remove_all(scriptsDirectory, fileError);
    return passed ? 0 : 1;
}

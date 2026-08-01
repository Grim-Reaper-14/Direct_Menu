#include "logging/Logger.hpp"
#include "scripting/LuaBindingLibrary.hpp"
#include "scripting/LuaCommands.hpp"
#include "scripting/LuaEvents.hpp"
#include "scripting/LuaTimerManager.hpp"
#include "scripting/LuaUI.hpp"

#include <imgui.h>
#include <sol/sol.hpp>

#include <iostream>
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
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {1280.0F, 720.0F};
    io.DeltaTime = 1.0F / 60.0F;
    unsigned char* pixels = nullptr;
    int textureWidth = 0;
    int textureHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &textureWidth, &textureHeight);

    smf::logging::LoggerApi logger;
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);
    smf::scripting::LuaCommands commands;
    smf::scripting::LuaEvents events{logger};
    smf::scripting::LuaTimerManager timers{logger};
    smf::scripting::LuaUI retainedUi{logger};
    bool drawingFrame = false;

    smf::scripting::LuaBindingLibrary bindings{
        logger,
        lua,
        commands,
        events,
        timers,
        retainedUi,
        [] { return std::string{"imgui-smoke"}; },
        [&drawingFrame] { return drawingFrame; },
        [] {}};
    bindings.RegisterCoreBindings();

    bool passed = true;
    const sol::protected_function_result outsideFrame = lua.safe_script(
        "return ImGui.Button('Outside frame')",
        sol::script_pass_on_error);
    passed &= Expect(outsideFrame.valid(), "Guarded outside-frame call raised an error.");
    if (outsideFrame.valid()) {
        passed &= Expect(!outsideFrame.get<bool>(), "Outside-frame ImGui call was not rejected.");
    }

    const sol::protected_function_result registration = lua.safe_script(
        R"(
            local enabled = false
            local slider = 0.25
            local input = "hello"

            event.on("draw", function()
                ImGui.SetNextWindowSize(420, 260, ImGui.Cond.FirstUseEver)
                local visible = ImGui.Begin(
                    "Lua ImGui Smoke",
                    ImGui.WindowFlags.AlwaysAutoResize)

                if visible then
                    ImGui.Text("Immediate-mode Lua binding is active")
                    ImGui.TextColored(0.2, 0.8, 1.0, 1.0, "Colored text")
                    local changed
                    changed, enabled = ImGui.Checkbox("Enabled", enabled)
                    changed, slider = ImGui.SliderFloat("Value", slider, 0.0, 1.0)
                    changed, input = ImGui.InputText("Input", input, 128)
                    ImGui.ProgressBar(slider)

                    if ImGui.BeginTabBar("SmokeTabs") then
                        if ImGui.BeginTabItem("One") then
                            ImGui.TextWrapped("Tab content from Lua")
                            ImGui.EndTabItem()
                        end
                        ImGui.EndTabBar()
                    end
                end

                ImGui.End()
                _G.IMGUI_SMOKE_DRAWN = true
            end)
        )",
        sol::script_pass_on_error);
    passed &= Expect(registration.valid(), "Lua draw callback registration failed.");
    if (!registration.valid()) {
        const sol::error error = registration;
        std::cerr << error.what() << '\n';
    }

    ImGui::NewFrame();
    drawingFrame = true;
    events.Emit("draw");
    drawingFrame = false;
    ImGui::Render();

    passed &= Expect(
        lua["IMGUI_SMOKE_DRAWN"].get_or(false),
        "Lua ImGui draw callback did not complete.");
    passed &= Expect(
        lua["ImGui"]["API_VERSION"].get_or(std::string{}) == "1.0.0",
        "ImGui API version was not exposed.");
    passed &= Expect(
        lua["imgui"]["WindowFlags"]["AlwaysAutoResize"].get_or(0) != 0,
        "Lowercase alias or ImGui flag constants were not exposed.");

    events.Clear();
    retainedUi.Clear();
    ImGui::DestroyContext();
    return passed ? 0 : 1;
}

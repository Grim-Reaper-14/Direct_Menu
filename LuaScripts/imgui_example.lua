SCRIPT_AUTHOR = "Tony"
SCRIPT_VERSION = "2.0.0"
SCRIPT_DESCRIPTION = "A multi-page Dear ImGui menu with network, session, and miscellaneous examples."

local enabled = false
local amount = 0.35
local message = "Hello from Lua"
local show_connection_status = true
local latency_overlay = false
local network_refresh_seconds = 5
local friends_only = false
local join_notifications = true
local maximum_results = 16
local show_clock = true
local compact_notifications = false
local interface_scale = 1.0

local function draw_home()
    ImGui.TextColored(0.35, 1.00, 0.55, 1.00, "Home")
    ImGui.TextWrapped(
        "This tabbed interface is rendered directly inside Settings > Lua " ..
        "by imgui_example.lua.")
    ImGui.Spacing()
    if ImGui.Button("Run Example Action", 180, 34) then
        log.info("Home action executed from the Lua ImGui menu")
    end
end

local function draw_network()
    ImGui.TextColored(0.25, 0.80, 1.00, 1.00, "Network")
    ImGui.TextWrapped("Local UI preferences only; this example does not contact a server.")
    local changed
    changed, show_connection_status = ImGui.Checkbox(
        "Show Connection Status", show_connection_status)
    changed, latency_overlay = ImGui.Checkbox("Latency Overlay", latency_overlay)
    changed, network_refresh_seconds = ImGui.SliderInt(
        "Refresh Seconds", network_refresh_seconds, 1, 60, "%d")
    if ImGui.Button("Refresh Network Status", 210, 34) then
        log.debug(
            "Network status refresh requested (demo), interval=" ..
            tostring(network_refresh_seconds))
    end
end

local function draw_sessions()
    ImGui.TextColored(0.75, 0.55, 1.00, 1.00, "Online Sessions")
    ImGui.TextWrapped("Session filters are demonstration state; no live sessions are accessed.")
    local changed
    changed, friends_only = ImGui.Checkbox("Friends-Only Filter", friends_only)
    changed, join_notifications = ImGui.Checkbox(
        "Join Notifications", join_notifications)
    changed, maximum_results = ImGui.SliderInt(
        "Maximum Results", maximum_results, 1, 32, "%d")
    if ImGui.Button("Refresh Session List", 200, 34) then
        log.info(
            "Session list refresh requested (demo), friends_only=" ..
            tostring(friends_only))
    end
    ImGui.SameLine()
    if ImGui.Button("Quick Join", 120, 34) then
        log.warn("Quick Join selected; no online provider is connected")
    end
end

local function draw_misc()
    ImGui.TextColored(1.00, 0.75, 0.25, 1.00, "Miscellaneous")
    local changed
    changed, show_clock = ImGui.Checkbox("Show Menu Clock", show_clock)
    changed, compact_notifications = ImGui.Checkbox(
        "Compact Notifications", compact_notifications)
    changed, interface_scale = ImGui.SliderFloat(
        "Interface Scale", interface_scale, 0.75, 1.50, "%.2f")
    ImGui.ProgressBar((interface_scale - 0.75) / 0.75)
    if ImGui.Button("Test Misc Notification", 200, 34) then
        log.info("Misc notification test from Lua")
    end
end

local function draw_controls()
    ImGui.TextColored(1.00, 0.75, 0.25, 1.00, "Controls")
    local changed
    changed, enabled = ImGui.Checkbox("Enabled", enabled)
    changed, amount = ImGui.SliderFloat("Amount", amount, 0.0, 1.0, "%.2f")
    changed, message = ImGui.InputText("Message", message, 256)
    ImGui.ProgressBar(amount)
    if ImGui.Button("Write Settings to Log", 190, 34) then
        log.info(message .. " | enabled=" .. tostring(enabled))
    end
end

local function draw_about()
    ImGui.TextColored(0.75, 0.55, 1.00, 1.00, "About")
    ImGui.TextWrapped(
        "Direct_Menu exposes Dear ImGui controls to isolated Lua scripts " ..
        "through the global ImGui table.")
    if ImGui.CollapsingHeader("API Details", ImGui.TreeNodeFlags.DefaultOpen) then
        ImGui.Text("Lua API: " .. tostring(LUA_API_VERSION))
        ImGui.Text("ImGui API: " .. tostring(ImGui.API_VERSION))
        ImGui.Text("Script: " .. tostring(SCRIPT_NAME))
    end
end

event.on("menu", function()
    ImGui.TextColored(0.25, 0.80, 1.00, 1.00, "Lua ImGui Example Menu")
    ImGui.Separator()

    if ImGui.BeginTabBar("DirectMenuLuaTabs") then
        if ImGui.BeginTabItem("Home") then
            draw_home()
            ImGui.EndTabItem()
        end
        if ImGui.BeginTabItem("Network") then
            draw_network()
            ImGui.EndTabItem()
        end
        if ImGui.BeginTabItem("Sessions") then
            draw_sessions()
            ImGui.EndTabItem()
        end
        if ImGui.BeginTabItem("Misc") then
            draw_misc()
            ImGui.EndTabItem()
        end
        if ImGui.BeginTabItem("Controls") then
            draw_controls()
            ImGui.EndTabItem()
        end
        if ImGui.BeginTabItem("About") then
            draw_about()
            ImGui.EndTabItem()
        end
        ImGui.EndTabBar()
    end
end)

function on_unload()
    log.info(SCRIPT_NAME .. " unloaded")
end

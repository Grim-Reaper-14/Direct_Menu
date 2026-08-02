local plugin = direct.plugin({
    name = "API v2 Example",
    author = "Direct Menu",
    version = "1.1.0",
    api = "2.1",
    description = "Demonstrates the Direct Menu Lua API v2 lifecycle, services, and filesystem sandbox.",
    permissions = {
        "ui",
        "events",
        "timers",
        "commands",
        "filesystem.read",
        "filesystem.write"
    }
})

direct.log.info("Loaded " .. SCRIPT_NAME .. " with API " .. direct.api_version.string)

direct.files.create_directory("settings")
if not direct.files.exists("settings/state.txt") then
    direct.files.write_text("settings/state.txt", "first run\n")
end

local saved = direct.files.read_text("settings/state.txt")
if saved then
    direct.log.debug("Sandbox state: " .. saved)
end

direct.ui.text("Direct Menu Lua API v2")

local enabled = false
local checkbox = direct.ui.checkbox("Example enabled", false, function(value)
    enabled = value
    direct.files.write_text("settings/enabled.txt", tostring(value))
    direct.log.info("Example enabled = " .. tostring(value))
end)

local button = direct.ui.button("Run Example Command", function()
    if direct.command.exists("example.command") then
        direct.command.execute("example.command")
    else
        direct.log.warn("example.command is not registered")
    end
end)

local tick = direct.events.on("tick", function()
    if not enabled then
        return
    end
end)

local heartbeat = direct.timer.every(5000, function(timer_id)
    direct.files.append_text("heartbeat.log", "timer " .. tostring(timer_id) .. "\n")
end)

function on_unload()
    direct.log.info("Unloading " .. SCRIPT_NAME)
    direct.events.off(tick)
    direct.timer.cancel(heartbeat)
    direct.ui.remove(button)
    direct.ui.remove(checkbox)
end

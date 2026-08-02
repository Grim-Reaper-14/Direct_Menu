local plugin = direct.plugin({
    name = "API v2 Example",
    author = "Direct Menu",
    version = "1.0.0",
    api = "2.0",
    description = "Demonstrates the Direct Menu Lua API v2 lifecycle and services.",
    permissions = {
        "ui",
        "events",
        "timers",
        "commands"
    }
})

direct.log.info("Loaded " .. SCRIPT_NAME .. " with API " .. direct.api_version.string)

direct.ui.text("Direct Menu Lua API v2")

local enabled = false
local checkbox = direct.ui.checkbox("Example enabled", false, function(value)
    enabled = value
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
    -- Per-frame work belongs here. Keep it light.
end)

local heartbeat = direct.timer.every(5000, function(timer_id)
    direct.log.debug("Heartbeat timer " .. tostring(timer_id))
end)

function on_unload()
    direct.log.info("Unloading " .. SCRIPT_NAME)
    direct.events.off(tick)
    direct.timer.cancel(heartbeat)
    direct.ui.remove(button)
    direct.ui.remove(checkbox)
end

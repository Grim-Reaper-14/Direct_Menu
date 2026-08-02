local plugin = direct.plugin({
    name = "Direct Menu Runtime Smoke Test",
    author = "Direct Menu",
    version = "1.0.0",
    api = "2.1",
    description = "Logs on load and adds a visible control to the Lua page.",
    permissions = {
        "ui",
        "events",
        "filesystem.write"
    }
})

direct.log.info("Runtime smoke script loaded: " .. SCRIPT_NAME)
direct.ui.text("Lua runtime smoke test is loaded.")
direct.ui.button("Test Lua Callback", function()
    direct.log.info("Runtime smoke button callback executed.")
end)

local first_tick
first_tick = direct.events.on("tick", function()
    direct.log.info("Runtime smoke tick callback executed.")
    direct.files.write_text("callback-ok.txt", "Lua callback executed\n")
    direct.events.off(first_tick)
end)

function on_unload()
    direct.log.info("Runtime smoke script unloaded: " .. SCRIPT_NAME)
end

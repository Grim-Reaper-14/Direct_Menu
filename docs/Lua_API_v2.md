# Direct Menu Lua API v2

Direct Menu embeds Lua 5.4 through sol2. Scripts run in their own `sol::environment` and resources created by a script are owned by that script so they can be cleaned up during unload or hot reload.

## Manifest

```lua
local plugin = direct.plugin({
    name = "My Plugin",
    author = "Author",
    version = "1.0.0",
    api = "2.0",
    description = "Example plugin",
    permissions = { "ui", "events", "timers" }
})
```

The manifest metadata is attached to the active `ScriptRecord`. Permissions are currently recorded for capability-gating work; sensitive bindings should not be added without checking these capabilities.

## API version

```lua
print(direct.api_version.string) -- 2.0.0
```

The legacy global tables remain available for compatibility, but new scripts should use the `direct` namespace.

## Logging

```lua
direct.log.trace("trace")
direct.log.debug("debug")
direct.log.info("info")
direct.log.warn("warning")
direct.log.error("error")
```

## Events

```lua
local id = direct.events.on("tick", function()
end)

direct.events.off(id)
direct.events.emit("my_event")
```

Built-in events currently include `tick`, `draw`, and `shutdown`.

## Timers

```lua
local once = direct.timer.after(1000, function(id)
end)

local repeating = direct.timer.every(5000, function(id)
end)

direct.timer.cancel(repeating)
```

## UI

```lua
local text = direct.ui.text("Hello")
local button = direct.ui.button("Run", function()
end)
local check = direct.ui.checkbox("Enabled", false, function(value)
end)

direct.ui.remove(button)
```

UI handles are automatically removed when their owning script unloads.

## Commands

```lua
if direct.command.exists("some.command") then
    direct.command.execute("some.command")
end

local commands = direct.command.list()
```

## Application

```lua
local script = direct.app.script_name()
local now = direct.app.time_ms()
direct.app.refresh_scripts()
```

## Native modules

```lua
if direct.module.exists("ExampleModule") then
    print(direct.module.version("ExampleModule"))
end

print(direct.module.count())
```

## Lifecycle

The script body is the load phase. Define `on_unload()` for explicit cleanup. Direct Menu also automatically removes script-owned commands, event handlers, timers, and UI widgets when a script unloads or hot reloads.

## Sandbox

The embedded runtime opens only base, math, string, table, coroutine, and UTF-8 libraries. `io`, `os`, `debug`, `package`, `require`, `dofile`, and `loadfile` are not exposed by default.

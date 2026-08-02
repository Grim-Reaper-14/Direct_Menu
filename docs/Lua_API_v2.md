# Direct Menu Lua API v2.1

Direct Menu embeds Lua 5.4 through sol2. Scripts run in their own `sol::environment`, and resources created by a script are owned by that script so they can be cleaned up during unload or hot reload.

## Manifest

```lua
local plugin = direct.plugin({
    name = "My Plugin",
    author = "Author",
    version = "1.0.0",
    api = "2.1",
    description = "Example plugin",
    permissions = {
        "ui",
        "events",
        "timers",
        "filesystem.read",
        "filesystem.write"
    }
})
```

Manifest metadata is attached to the active `ScriptRecord`. Filesystem permissions are enforced by the binding layer. `filesystem` grants both read and write access; `filesystem.read` and `filesystem.write` can be requested separately.

## API version

```lua
print(direct.api_version.string) -- 2.1.0
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

## Filesystem sandbox

Every Lua script gets a private filesystem root under:

```text
LuaScripts/.sandbox/<sanitized-script-name>/
```

Lua never receives unrestricted OS filesystem APIs. Absolute paths, drive-root paths, and `..` traversal are rejected. Existing junctions and symlinks are canonicalized and rejected if they resolve outside the owning script's sandbox.

The default text read/write limit is 1 MiB per operation/file.

### Read-only access

Declare:

```lua
permissions = { "filesystem.read" }
```

Then use:

```lua
if direct.files.exists("settings/state.txt") then
    local text = direct.files.read_text("settings/state.txt")
end

local entries = direct.files.list("settings")
local is_dir = direct.files.is_directory("settings")
```

### Write access

Declare:

```lua
permissions = { "filesystem.write" }
```

Then use:

```lua
direct.files.create_directory("settings")
direct.files.write_text("settings/state.txt", "enabled=true\n")
direct.files.append_text("logs/history.txt", "started\n")
direct.files.remove("old/cache.txt")
```

A script may request both permissions or use the broader `filesystem` permission.

For example, a script called `weather.lua` writing `cache/data.txt` is contained approximately as:

```text
LuaScripts/.sandbox/weather_lua/cache/data.txt
```

It cannot use `../../`, `C:\\...`, a UNC/rooted path, or a junction/symlink to escape that directory.

## Lifecycle

The script body is the load phase. Define `on_unload()` for explicit cleanup. Direct Menu also automatically removes script-owned commands, event handlers, timers, and UI widgets when a script unloads or hot reloads.

While callbacks run, Direct Menu restores the callback owner's script context. This means permission checks continue to work correctly inside event, timer, and Lua UI callbacks.

## Runtime sandbox

The embedded runtime opens only base, math, string, table, coroutine, and UTF-8 libraries. `io`, `os`, `debug`, `package`, `require`, `dofile`, and `loadfile` are not exposed by default.

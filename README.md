# Direct_Menu

Direct_Menu is an original, bare-bones Windows menu foundation built with
C++20, Win32, Direct3D 12, and Dear ImGui. It uses a compact nested side-menu
layout, while keeping the renderer, application core, UI, feature registry, and
services separated.

The feature pages are intentionally UI-only placeholders. The project does not
inject into another process, read or write game memory, bypass protections, or
unlock online content.

## Included

- Direct3D 12 renderer with frame synchronization and descriptor allocation
- Welcome screen shown before the main menu
- Compact nested navigation with Self, Weapons, Teleport, Unlocks, Vehicle,
  Vehicle Spawn, LSC, and Settings pages
- Global `F5` hotkey that hides and reopens the running menu
- Central typed feature registry
- Save and load configuration files
- Live themes and live font selection
- WIC-based PNG/JPG/BMP image loading and preview
- Thread-safe logger API
- C++20 background task queue with clean shutdown and error callbacks
- Filesystem manager that creates the runtime folders
- Lua manager placeholder under Settings, ready for binding after the native
  feature API is finalized

## Build on Windows

Requirements:

- Windows 10 or Windows 11
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.24 or newer
- Git, used by CMake to download the pinned Dear ImGui, Lua 5.4, and sol2 dependencies

The easiest method is to double-click `build_windows.bat`.

From PowerShell:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-release
.\build\Release\Direct_Menu.exe
```

The first configure downloads Dear ImGui `v1.91.9b`, Lua `v5.4.7`, and sol2
`v3.3.1`. Later builds reuse the downloaded dependencies.

If the project is pushed to GitHub, the included **Windows Build** workflow can
compile it from a phone or browser. Run the workflow, then download the
`Direct_Menu-Windows-x64` artifact from that workflow run.

## Runtime files

On first launch the framework creates:

```text
%LOCALAPPDATA%\Direct_Menu\
├── Configurations\
├── Fonts\
├── Images\
├── Logs\
└── LuaScripts\
```

`F5` only changes visibility. Use **Settings > Exit Menu** to terminate the
program.

The **Settings > Lua** page can refresh the script folder, open it in File
Explorer, load/unload/reload individual scripts, reload all active scripts,
and show protected runtime errors. Scripts auto-load by default and loaded
files hot-reload when they change on disk. A script can expose UI metadata
with globals such as `SCRIPT_AUTHOR`, `SCRIPT_VERSION`, and
`SCRIPT_DESCRIPTION`, or with a `SCRIPT_METADATA` table containing `author`,
`version`, and `description` fields.

### Lua ImGui API

Scripts can draw immediate-mode interfaces by registering a protected `draw`
event callback. The global `ImGui` table follows Dear ImGui naming, and the
same table is also available as lowercase `imgui`:

```lua
local enabled = false

event.on("draw", function()
    ImGui.SetNextWindowSize(420, 240, ImGui.Cond.FirstUseEver)
    local visible = ImGui.Begin(
        "My Lua Window",
        ImGui.WindowFlags.AlwaysAutoResize)

    if visible then
        ImGui.Text("Hello from Lua")
        local changed
        changed, enabled = ImGui.Checkbox("Enabled", enabled)
        if ImGui.Button("Run") then
            log.info("Lua ImGui button clicked")
        end
    end

    ImGui.End()
end)
```

The binding includes windows, text, buttons, checkboxes, radio buttons,
sliders, numeric/text inputs, color editing, progress bars, tree/header and
selectable controls, tabs, combos, common layout helpers, item-state queries,
tooltips, and common flag tables. Immediate-mode functions are intentionally
guarded so they only run from a `draw` callback or a retained Lua UI callback.

## Project structure

```text
src/
├── backend/      Direct3D 12 device, swap chain, frames, descriptors, textures
├── config/       Settings persistence
├── core/         Win32 lifetime, message handling, and background task queue
├── features/     Typed feature registry shared by UI/configs/future Lua
├── filesystem/   Runtime directories and file dialogs
├── logging/      Thread-safe logger API
├── scripting/    Lua 5.4 runtime, lifecycle, bindings, events, timers, and UI
└── ui/           Menu, themes, fonts, notifications, and image loading
```

The correct order for adding real application features is:

1. Register the native feature in `FeatureRegistry`.
2. Connect it to a legitimate application or single-player mod provider.
3. Confirm configuration save/load behavior.
4. Bind the same registry entry to Lua last.

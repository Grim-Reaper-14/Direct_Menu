cmake_minimum_required(VERSION 3.24)

project(
    Direct_Menu
    VERSION 0.2.0
    DESCRIPTION "A modular Win32, Direct3D 12, Dear ImGui, and Lua side-menu framework"
    LANGUAGES C CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "Direct_Menu targets Windows 10/11 and Direct3D 12.")
endif()

enable_language(RC)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sol2)

add_library(lua_static STATIC
    "${lua_SOURCE_DIR}/lapi.c"
    "${lua_SOURCE_DIR}/lauxlib.c"
    "${lua_SOURCE_DIR}/lbaselib.c"
    "${lua_SOURCE_DIR}/lcode.c"
    "${lua_SOURCE_DIR}/lcorolib.c"
    "${lua_SOURCE_DIR}/lctype.c"
    "${lua_SOURCE_DIR}/ldblib.c"
    "${lua_SOURCE_DIR}/ldebug.c"
    "${lua_SOURCE_DIR}/ldo.c"
    "${lua_SOURCE_DIR}/ldump.c"
    "${lua_SOURCE_DIR}/lfunc.c"
    "${lua_SOURCE_DIR}/lgc.c"
    "${lua_SOURCE_DIR}/linit.c"
    "${lua_SOURCE_DIR}/liolib.c"
    "${lua_SOURCE_DIR}/llex.c"
    "${lua_SOURCE_DIR}/lmathlib.c"
    "${lua_SOURCE_DIR}/lmem.c"
    "${lua_SOURCE_DIR}/loadlib.c"
    "${lua_SOURCE_DIR}/lobject.c"
    "${lua_SOURCE_DIR}/lopcodes.c"
    "${lua_SOURCE_DIR}/loslib.c"
    "${lua_SOURCE_DIR}/lparser.c"
    "${lua_SOURCE_DIR}/lstate.c"
    "${lua_SOURCE_DIR}/lstring.c"
    "${lua_SOURCE_DIR}/lstrlib.c"
    "${lua_SOURCE_DIR}/ltable.c"
    "${lua_SOURCE_DIR}/ltablib.c"
    "${lua_SOURCE_DIR}/ltm.c"
    "${lua_SOURCE_DIR}/lundump.c"
    "${lua_SOURCE_DIR}/lutf8lib.c"
    "${lua_SOURCE_DIR}/lvm.c"
    "${lua_SOURCE_DIR}/lzio.c"
)
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}")
set_target_properties(lua_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
)

target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)

target_compile_definitions(imgui PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    UNICODE
    _UNICODE
)

set(DIRECT_MENU_NATIVE_SOURCES
    src/natives/NativeInvoker.cpp
    src/natives/NativeQueue.cpp
    src/natives/NativeRegistry.cpp
    src/natives/NativeScheduler.cpp
)

set(DIRECT_MENU_SDK_SOURCES
    src/sdk/Entity.cpp
    src/sdk/SDK.cpp
)

add_executable(Direct_Menu WIN32
    src/resources/Direct_Menu.rc
    src/main.cpp
    src/backend/D3D12Backend.cpp
    src/config/ConfigManager.cpp
    src/core/Application.cpp
    src/core/File_System_Monitor.cpp
    src/core/ImGuiStyleSettings.cpp
    src/core/Logger.cpp
    src/core/MemoryManagerAPI.cpp
    src/core/PatternCache.cpp
    src/core/SignatureManager.cpp
    src/core/TaskQueue.cpp
    src/features/FeatureRegistry.cpp
    src/filesystem/FileSystemManager.cpp
    src/logging/Logger.cpp
    ${DIRECT_MENU_NATIVE_SOURCES}
    ${DIRECT_MENU_SDK_SOURCES}
    src/providers/GameProvider.cpp
    src/scripting/LuaBindingLibrary.cpp
    src/scripting/LuaCommands.cpp
    src/scripting/LuaEvents.cpp
    src/scripting/LuaFileSystemSandbox.cpp
    src/scripting/LuaImGuiBindings.cpp
    src/scripting/LuaManager.cpp
    src/scripting/LuaMemoryBindings.cpp
    src/scripting/LuaModule.cpp
    src/scripting/LuaModuleManager.cpp
    src/scripting/LuaScriptsManager.cpp
    src/scripting/LuaTimerManager.cpp
    src/scripting/LuaUI.cpp
    src/ui/FontManager.cpp
    src/ui/ImageLoader.cpp
    src/ui/Menu.cpp
    src/ui/NotificationCenter.cpp
    src/ui/ThemeManager.cpp
)

target_include_directories(Direct_Menu PRIVATE src)
target_compile_features(Direct_Menu PRIVATE cxx_std_20)

target_compile_definitions(Direct_Menu PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(Direct_Menu PRIVATE
        /W4
        /permissive-
        /EHsc
        /utf-8
        /Zc:__cplusplus
    )
endif()

target_link_libraries(Direct_Menu PRIVATE
    imgui
    lua_static
    sol2::sol2
    d3d12
    dxgi
    dxguid
    d3dcompiler
    dwmapi
    windowscodecs
    ole32
    shell32
    comdlg32
)

set_target_properties(Direct_Menu PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Direct_Menu>"
)

add_custom_command(TARGET Direct_Menu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Direct_Menu>/assets"
    COMMENT "Preparing Direct_Menu assets"
)

set(DIRECT_MENU_RUNTIME_ASSETS
    direct_menu_neon_background.jpg
    direct_menu_background.png
    direct_menu_header.png
    grim_reaper_icon.png
)

foreach(asset_name IN LISTS DIRECT_MENU_RUNTIME_ASSETS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/${asset_name}")
        add_custom_command(TARGET Direct_Menu POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/${asset_name}"
                "$<TARGET_FILE_DIR:Direct_Menu>/assets/${asset_name}"
        )
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(FeatureRegistrySmoke
        tests/FeatureRegistrySmoke.cpp
        src/features/FeatureRegistry.cpp
    )
    target_include_directories(FeatureRegistrySmoke PRIVATE src)
    target_compile_features(FeatureRegistrySmoke PRIVATE cxx_std_20)
    add_test(NAME FeatureRegistrySmoke COMMAND FeatureRegistrySmoke)

    add_executable(TaskQueueSmoke
        tests/TaskQueueSmoke.cpp
        src/core/TaskQueue.cpp
    )
    target_include_directories(TaskQueueSmoke PRIVATE src)
    target_compile_features(TaskQueueSmoke PRIVATE cxx_std_20)
    add_test(NAME TaskQueueSmoke COMMAND TaskQueueSmoke)

    add_executable(ImGuiStyleSettingsSmoke
        tests/ImGuiStyleSettingsSmoke.cpp
        src/core/ImGuiStyleSettings.cpp
    )
    target_include_directories(ImGuiStyleSettingsSmoke PRIVATE src)
    target_compile_features(ImGuiStyleSettingsSmoke PRIVATE cxx_std_20)
    target_link_libraries(ImGuiStyleSettingsSmoke PRIVATE imgui)
    add_test(NAME ImGuiStyleSettingsSmoke COMMAND ImGuiStyleSettingsSmoke)

    add_executable(NativeRuntimeSmoke
        tests/NativeRuntimeSmoke.cpp
        ${DIRECT_MENU_NATIVE_SOURCES}
    )
    target_include_directories(NativeRuntimeSmoke PRIVATE src)
    target_compile_features(NativeRuntimeSmoke PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(NativeRuntimeSmoke PRIVATE
            /W4
            /permissive-
            /EHsc
            /utf-8
            /Zc:__cplusplus
        )
    endif()
    add_test(NAME NativeRuntimeSmoke COMMAND NativeRuntimeSmoke)

    add_executable(SDKSmoke
        tests/SDKSmoke.cpp
        ${DIRECT_MENU_SDK_SOURCES}
        src/core/Logger.cpp
        src/core/MemoryManagerAPI.cpp
        src/core/SignatureManager.cpp
        src/logging/Logger.cpp
        src/natives/NativeQueue.cpp
        src/natives/NativeScheduler.cpp
    )
    target_include_directories(SDKSmoke PRIVATE src)
    target_compile_features(SDKSmoke PRIVATE cxx_std_20)
    target_compile_definitions(SDKSmoke PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
        UNICODE
        _UNICODE
    )
    if(MSVC)
        target_compile_options(SDKSmoke PRIVATE
            /W4
            /permissive-
            /EHsc
            /utf-8
            /Zc:__cplusplus
        )
    endif()
    add_test(NAME SDKSmoke COMMAND SDKSmoke)

    add_executable(LuaRuntimeSmoke
        tests/LuaRuntimeSmoke.cpp
        src/core/Logger.cpp
        src/core/MemoryManagerAPI.cpp
        src/core/SignatureManager.cpp
        src/features/FeatureRegistry.cpp
        src/logging/Logger.cpp
        src/scripting/LuaBindingLibrary.cpp
        src/scripting/LuaCommands.cpp
        src/scripting/LuaEvents.cpp
        src/scripting/LuaFileSystemSandbox.cpp
        src/scripting/LuaImGuiBindings.cpp
        src/scripting/LuaManager.cpp
        src/scripting/LuaMemoryBindings.cpp
        src/scripting/LuaModule.cpp
        src/scripting/LuaModuleManager.cpp
        src/scripting/LuaScriptsManager.cpp
        src/scripting/LuaTimerManager.cpp
        src/scripting/LuaUI.cpp
    )
    target_include_directories(LuaRuntimeSmoke PRIVATE src)
    target_compile_features(LuaRuntimeSmoke PRIVATE cxx_std_20)
    target_compile_definitions(LuaRuntimeSmoke PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
        UNICODE
        _UNICODE
    )
    if(MSVC)
        target_compile_options(LuaRuntimeSmoke PRIVATE
            /W4
            /permissive-
            /EHsc
            /utf-8
            /Zc:__cplusplus
        )
    endif()
    target_link_libraries(LuaRuntimeSmoke PRIVATE
        imgui
        lua_static
        sol2::sol2
    )
    add_test(NAME LuaRuntimeSmoke COMMAND LuaRuntimeSmoke)
endif()

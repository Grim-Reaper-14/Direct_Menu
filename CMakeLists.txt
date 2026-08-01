cmake_minimum_required(VERSION 3.24)

project(
    Direct_Menu
    VERSION 0.1.0
    DESCRIPTION "A modular Win32, Direct3D 12, and Dear ImGui side-menu framework"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "Direct_Menu targets Windows 10/11 and Direct3D 12.")
endif()

include(FetchContent)

# Pinned so the renderer API does not unexpectedly change between builds.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

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

add_executable(Direct_Menu WIN32
    src/main.cpp
    src/backend/D3D12Backend.cpp
    src/config/ConfigManager.cpp
    src/core/Application.cpp
    src/core/TaskQueue.cpp
    src/features/FeatureRegistry.cpp
    src/filesystem/FileSystemManager.cpp
    src/logging/Logger.cpp
    src/scripting/LuaManager.cpp
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
    direct_menu_background.jpg
    direct_menu_header.jpg
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
endif()

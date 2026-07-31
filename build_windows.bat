@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found. Install Visual Studio 2022 with Desktop development with C++ and CMake tools.
    pause
    exit /b 1
)

cmake --preset windows-msvc
if errorlevel 1 (
    echo Configure failed.
    pause
    exit /b 1
)

cmake --build --preset windows-release
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build complete:
echo build\Release\Direct_Menu.exe
pause

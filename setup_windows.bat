@echo off
setlocal enabledelayedexpansion

echo === Vulkan Engine Windows Setup ===
echo.

setlocal enabledelayedexpansion

echo [1/5] Checking for required tools...

where xmake >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] xmake found
) else (
    echo [MISSING] xmake - https://xmake.io
)

where glslc >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] glslc found
) else (
    echo [MISSING] glslc from Vulkan SDK - https://vulkan.lunarg.com
)

where clang-format >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] clang-format found
) else (
    echo [OPTIONAL] clang-format not found
)

echo.
echo [2/5] Checking for C++ compiler...

set compiler_found=0

where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] MSVC found
    set compiler_found=1
)

where clang++ >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Clang found
    set compiler_found=1
)

where g++ >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] GCC/MinGW found
    set compiler_found=1
)

if %compiler_found% equ 0 (
    echo [ERROR] No C++ compiler found
    echo Install one of: Visual Studio 2022, Clang/LLVM, or MinGW-w64
    exit /b 1
)

echo.
echo [3/5] Checking environment variables...

if defined VULKAN_SDK (
    echo [OK] VULKAN_SDK = !VULKAN_SDK!
) else (
    echo [OPTIONAL] VULKAN_SDK not set
)

echo.
echo [4/5] Checking project structure...

if exist src (echo [OK] src) else (echo [MISSING] src)
if exist include (echo [OK] include) else (echo [MISSING] include)
if exist assets\shaders (echo [OK] assets\shaders) else (echo [MISSING] assets\shaders)
if exist assets\textures (echo [OK] assets\textures) else (echo [MISSING] assets\textures)
if exist assets\models (echo [OK] assets\models) else (echo [MISSING] assets\models)

echo.
echo [5/5] Summary
echo.

if %compiler_found% equ 1 (
    echo All critical components found! Ready to build.
    echo.
    echo Next steps:
    echo   xmake f -p windows -a x64 -m debug
    echo   xmake build Cube
    echo   xmake run Cube
) else (
    echo Some components missing. See above.
)

endlocal

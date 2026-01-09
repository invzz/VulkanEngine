#!/usr/bin/env pwsh
param()

Write-Host "=== Vulkan Engine Windows Setup ===" -ForegroundColor Cyan
Write-Host ""

$toolsFound = @{}
$compilerFound = $false

Write-Host "[1/5] Checking for required tools..." -ForegroundColor Magenta

if (Get-Command xmake -ErrorAction SilentlyContinue) {
    Write-Host "✓ xmake found" -ForegroundColor Green
    $toolsFound["xmake"] = $true
}
else {
    Write-Host "✗ xmake not found" -ForegroundColor Red
}

if (Get-Command glslc -ErrorAction SilentlyContinue) {
    Write-Host "✓ glslc found" -ForegroundColor Green
    $toolsFound["glslc"] = $true
}
else {
    Write-Host "✗ glslc not found (install Vulkan SDK)" -ForegroundColor Red
}

if (Get-Command clang-format -ErrorAction SilentlyContinue) {
    Write-Host "✓ clang-format found" -ForegroundColor Green
    $toolsFound["clang-format"] = $true
}
else {
    Write-Host "⚠ clang-format not found (optional)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "[2/5] Checking for C++ compiler..." -ForegroundColor Magenta

if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
    Write-Host "✓ MSVC found" -ForegroundColor Green
    $compilerFound = $true
}

if (Get-Command clang++ -ErrorAction SilentlyContinue) {
    Write-Host "✓ Clang found" -ForegroundColor Green
    $compilerFound = $true
}

if (Get-Command g++ -ErrorAction SilentlyContinue) {
    Write-Host "✓ GCC/MinGW found" -ForegroundColor Green
    $compilerFound = $true
}

if (-not $compilerFound) {
    Write-Host ""
    Write-Host "✗ No C++ compiler found!" -ForegroundColor Red
    Write-Host "Install one of: Visual Studio 2022, Clang/LLVM, or MinGW-w64" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "[3/5] Checking environment variables..." -ForegroundColor Magenta

if ($env:VULKAN_SDK) {
    Write-Host "✓ VULKAN_SDK set: $($env:VULKAN_SDK)" -ForegroundColor Green
}
else {
    Write-Host "⚠ VULKAN_SDK not set" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "[4/5] Checking project structure..." -ForegroundColor Magenta

$dirs = @("src", "include", "assets/shaders", "assets/textures", "assets/models")
foreach ($dir in $dirs) {
    if (Test-Path $dir) {
        Write-Host "✓ $dir" -ForegroundColor Green
    }
    else {
        Write-Host "✗ $dir missing" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "[5/5] Summary" -ForegroundColor Magenta
Write-Host ""

if ($toolsFound["xmake"] -and $compilerFound -and $toolsFound["glslc"]) {
    Write-Host "All critical components found! Ready to build." -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  xmake f -p windows -a x64 -m debug" -ForegroundColor White
    Write-Host "  xmake build Cube" -ForegroundColor White
    Write-Host "  xmake run Cube" -ForegroundColor White
}
else {
    Write-Host "Some components are missing:" -ForegroundColor Yellow
    if (-not $toolsFound["xmake"]) { Write-Host "  - xmake: https://xmake.io" }
    if (-not $compilerFound) { Write-Host "  - C++ Compiler: Visual Studio, Clang, or MinGW" }
    if (-not $toolsFound["glslc"]) { Write-Host "  - glslc: https://vulkan.lunarg.com" }
}

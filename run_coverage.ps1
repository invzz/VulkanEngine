#!/usr/bin/env pwsh
# ============================================================================
# Code Coverage Script for VulkanEngine
# Uses OpenCppCoverage for MSVC binaries (no special compiler flags needed)
# ============================================================================

param(
    [switch]$Clean,
    [switch]$Open,
    [string]$Filter = "",
    [string]$Mode = "debug"  # debug, release, or coverage
)

$ErrorActionPreference = "Stop"
$ProjectDir = $PSScriptRoot
$BuildDir = Join-Path $ProjectDir "build\windows\x64\$Mode"
$CoverageDir = Join-Path $ProjectDir "coverage"
$TestBinary = Join-Path $BuildDir "Tests.exe"
$CoberturaFile = Join-Path $CoverageDir "coverage.xml"
$LcovFile = Join-Path $CoverageDir "lcov.info"
$HtmlDir = Join-Path $CoverageDir "html"

# Check for OpenCppCoverage
$OpenCppCoverage = Get-Command "OpenCppCoverage" -ErrorAction SilentlyContinue
if (-not $OpenCppCoverage) {
    # Try common install locations
    $CommonPaths = @(
        "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe",
        "C:\Program Files (x86)\OpenCppCoverage\OpenCppCoverage.exe",
        "$env:LOCALAPPDATA\Programs\OpenCppCoverage\OpenCppCoverage.exe"
    )
    foreach ($p in $CommonPaths) {
        if (Test-Path $p) {
            $OpenCppCoverage = $p
            break
        }
    }
    
    if (-not $OpenCppCoverage) {
        Write-Host "OpenCppCoverage not found. Install it from:" -ForegroundColor Yellow
        Write-Host "  https://github.com/OpenCppCoverage/OpenCppCoverage/releases" -ForegroundColor Cyan
        Write-Host "  or: winget install OpenCppCoverage.OpenCppCoverage" -ForegroundColor Cyan
        exit 1
    }
}

# Clean previous coverage data
if ($Clean -or -not (Test-Path $CoverageDir)) {
    Write-Host "Cleaning coverage data..." -ForegroundColor Cyan
    if (Test-Path $CoverageDir) { Remove-Item -Recurse -Force $CoverageDir }
}
New-Item -ItemType Directory -Path $CoverageDir -Force | Out-Null

# Configure and build
Write-Host "`n=== Building project (mode: $Mode) ===" -ForegroundColor Green
xmake f -p windows -a x64 -m $Mode -y
if ($LASTEXITCODE -ne 0) { 
    Write-Error "xmake configure failed"
    exit 1 
}

xmake build Tests
if ($LASTEXITCODE -ne 0) { 
    Write-Error "xmake build failed"
    exit 1 
}

# Find the actual test binary location
$TestBinarySearch = Get-ChildItem -Path (Join-Path $ProjectDir "build") -Recurse -Filter "Tests.exe" | 
    Where-Object { $_.DirectoryName -like "*\$Mode*" -or $_.DirectoryName -like "*$Mode*" } |
    Select-Object -First 1
if ($TestBinarySearch) {
    $TestBinary = $TestBinarySearch.FullName
    Write-Host "Found test binary: $TestBinary" -ForegroundColor Cyan
} elseif (Test-Path $TestBinary) {
    Write-Host "Using test binary: $TestBinary" -ForegroundColor Cyan
} else {
    # Fallback: find any Tests.exe
    $TestBinarySearch = Get-ChildItem -Path (Join-Path $ProjectDir "build") -Recurse -Filter "Tests.exe" | Select-Object -First 1
    if ($TestBinarySearch) {
        $TestBinary = $TestBinarySearch.FullName
        Write-Host "Found test binary (fallback): $TestBinary" -ForegroundColor Yellow
    } else {
        Write-Error "Tests.exe not found in build directory"
        exit 1
    }
}

# Build test arguments
$TestArgs = @()
if ($Filter) {
    $TestArgs += "--gtest_filter=$Filter"
}

# Run tests with coverage using OpenCppCoverage
Write-Host "`n=== Running tests with coverage ===" -ForegroundColor Green

# Source directories to include in coverage
$SourceDirs = @(
    (Join-Path $ProjectDir "src\Engine"),
    (Join-Path $ProjectDir "src\EngineSceneIO"),
    (Join-Path $ProjectDir "src\ModelLib")
)

# Build OpenCppCoverage arguments
$CovArgs = @(
    "--export_type=cobertura:$CoberturaFile",
    "--export_type=html:$HtmlDir",
    # Only instrument our modules, not system DLLs
    "--modules=$BuildDir",
    # Verbose output for debugging
    "--quiet"
)

# Add source directories for filtering
foreach ($dir in $SourceDirs) {
    if (Test-Path $dir) {
        $CovArgs += "--sources=$dir"
    }
}

# Also add include directory for header-only code
$IncludeDirs = @(
    (Join-Path $ProjectDir "include\Engine"),
    (Join-Path $ProjectDir "include\EngineSceneIO"),
    (Join-Path $ProjectDir "include\ModelLib")
)
foreach ($dir in $IncludeDirs) {
    if (Test-Path $dir) {
        $CovArgs += "--sources=$dir"
    }
}

# Exclude test code and third-party
$CovArgs += "--excluded_sources=*\tests\*"
$CovArgs += "--excluded_sources=*\third_party\*"
$CovArgs += "--excluded_sources=*\build\*"
$CovArgs += "--excluded_sources=*\.xmake\*"
$CovArgs += "--excluded_sources=*\stb\*"
$CovArgs += "--excluded_sources=*\imgui\*"

# Add the test binary and its arguments
$CovArgs += "--"
$CovArgs += $TestBinary
$CovArgs += $TestArgs

Write-Host "Running: OpenCppCoverage $($CovArgs -join ' ')" -ForegroundColor DarkGray
& $OpenCppCoverage @CovArgs
$TestExitCode = $LASTEXITCODE

# Convert Cobertura to LCOV format for VS Code Coverage Gutters extension
if (Test-Path $CoberturaFile) {
    Write-Host "`nConverting to LCOV format..." -ForegroundColor Cyan
    
    # Simple Cobertura to LCOV converter (basic implementation)
    try {
        [xml]$cobertura = Get-Content $CoberturaFile
        $lcovContent = @()
        
        foreach ($package in $cobertura.coverage.packages.package) {
            foreach ($class in $package.classes.class) {
                $filename = $class.filename -replace '\\', '/'
                if ($filename) {
                    $lcovContent += "SF:$filename"
                    foreach ($line in $class.lines.line) {
                        $lcovContent += "DA:$($line.number),$($line.hits)"
                    }
                    $lcovContent += "end_of_record"
                }
            }
        }
        
        $lcovContent | Out-File -FilePath $LcovFile -Encoding UTF8
        Write-Host "LCOV file created: $LcovFile" -ForegroundColor Green
    } catch {
        Write-Host "Warning: Could not convert to LCOV format: $_" -ForegroundColor Yellow
    }
}

# Print summary
Write-Host "`n=== Output Files ===" -ForegroundColor Cyan
Write-Host "  Cobertura XML: $CoberturaFile"
Write-Host "  LCOV:          $LcovFile"
Write-Host "  HTML Report:   $HtmlDir\index.html"

# Open HTML report if requested
if ($Open) {
    $indexPath = Join-Path $HtmlDir "index.html"
    if (Test-Path $indexPath) {
        Start-Process $indexPath
    }
}

# Return test exit code
exit $TestExitCode

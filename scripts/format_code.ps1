#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Formats C++ code using clang-format
.DESCRIPTION
    Windows PowerShell version of format_code.sh
    Recursively formats all C++ source files in the project
#>

Write-Host "[" -NoNewline -ForegroundColor White
Write-Host " Formatting " -NoNewline -ForegroundColor Green
Write-Host "] C++ code..." -ForegroundColor White

$excludePaths = @("build", ".xmake", ".git", "assets", "BLENDER", "tools")

# Get all C++ files, excluding build directories
$cppFiles = Get-ChildItem -Path . -Recurse -Include *.cpp, *.hpp, *.h -ErrorAction SilentlyContinue | 
    Where-Object { 
        $excluded = $false
        foreach ($exclude in $excludePaths) {
            if ($_.FullName -like "*\$exclude\*") {
                $excluded = $true
                break
            }
        }
        -not $excluded
    }

if ($cppFiles.Count -eq 0) {
    Write-Host "No C++ files found to format."
    exit 0
}

Write-Host "Found $($cppFiles.Count) C++ files to format" -ForegroundColor Cyan

$formatted = 0
$failed = 0

foreach ($file in $cppFiles) {
    try {
        clang-format -i $file.FullName 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  Formatted: $($file.Name)" -ForegroundColor DarkGreen
            $formatted++
        } else {
            Write-Host "  Failed to format: $($file.Name)" -ForegroundColor DarkRed
            $failed++
        }
    } catch {
        Write-Host "[" -NoNewline -ForegroundColor White
        Write-Host " Error " -NoNewline -ForegroundColor Red
        Write-Host "] clang-format not found. Install it with: winget install LLVM.LLVM" -ForegroundColor White
        exit 1
    }
}

Write-Host ""
Write-Host "[" -NoNewline -ForegroundColor White
Write-Host " Formatting " -NoNewline -ForegroundColor Green
Write-Host "] Complete. Formatted: $formatted, Failed: $failed" -ForegroundColor White

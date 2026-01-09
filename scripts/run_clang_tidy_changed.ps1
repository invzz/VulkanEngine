#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Run clang-tidy on C/C++ source files changed vs a base ref.

.DESCRIPTION
  - Collects changed files via git diff.
  - Filters to compilable translation units (.c/.cc/.cpp/.cxx).
  - Runs clang-tidy using the compile_commands.json compilation database.

.PARAMETER Base
  Git ref to diff against (default: auto-detect origin/main, main, origin/master, master; fallback HEAD~1).

.PARAMETER CompileCommandsDir
  Directory containing compile_commands.json (default: repo root).

.PARAMETER Fix
  If set, runs clang-tidy with -fix.

.EXAMPLE
  ./scripts/run_clang_tidy_changed.ps1

.EXAMPLE
  ./scripts/run_clang_tidy_changed.ps1 -Base main
#>

[CmdletBinding()]
param(
    [string]$Base = "",
    [string]$CompileCommandsDir = ".",
    [int]$Jobs = 1,
    [switch]$Fix
)

$ErrorActionPreference = "Stop"

function Test-GitRef {
    param([Parameter(Mandatory = $true)][string]$Ref)
    try {
        git rev-parse --verify "$Ref^{commit}" *> $null
        return $true
    }
    catch {
        return $false
    }
}

try {
    git --version *> $null
}
catch {
    Write-Error "git is required to run this script."
    exit 1
}

if (-not (Test-Path (Join-Path $CompileCommandsDir "compile_commands.json"))) {
    Write-Error "compile_commands.json not found in '$CompileCommandsDir'. Run: xmake project -k compile_commands"
    exit 1
}

if (-not $Base) {
    foreach ($candidate in @("origin/main", "main", "origin/master", "master")) {
        if (Test-GitRef $candidate) { $Base = $candidate; break }
    }
    if (-not $Base) {
        $Base = "HEAD~1"
    }
}

if (-not (Test-GitRef $Base)) {
    Write-Error "Base ref '$Base' not found. Provide -Base <ref>."
    exit 1
}

$files = git diff --name-only --diff-filter=ACMRTUXB "$Base...HEAD"
if (-not $files) {
    Write-Host "No changed files vs $Base." -ForegroundColor Yellow
    exit 0
}

$tuFiles = @(
    $files |
    Where-Object { $_ -match '^(src|include)[/\\]' } |
    Where-Object { $_ -match '\.(c|cc|cpp|cxx)$' }
)
if ($tuFiles.Count -eq 0) {
    Write-Host "No changed C/C++ translation units (.c/.cc/.cpp/.cxx) vs $Base." -ForegroundColor Yellow
    exit 0
}

Write-Host "Running clang-tidy on $($tuFiles.Count) file(s) vs $Base" -ForegroundColor Green

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot ".."))
$repoRootRegex = [Regex]::Escape($repoRoot.Path.Replace('\\', '/'))
$headerFilter = "^$repoRootRegex/(include|src)/"

$tidyArgsBase = @(
    "-p", $CompileCommandsDir,
    "-quiet",
    "--extra-arg=-w"
)
if ($Fix) {
    $tidyArgsBase += "-fix"
}

$tidyArgsBase += "--header-filter=$headerFilter"

$failed = 0

if ($Jobs -gt 1 -and $PSVersionTable.PSVersion.Major -ge 7) {
    $results = $tuFiles | ForEach-Object -Parallel {
        param($file, $compileDbDir, $headerFilter, $doFix)
        $args = @(
            "-p", $compileDbDir,
            "-quiet",
            "--extra-arg=-w",
            "--header-filter=$headerFilter"
        )
        if ($doFix) { $args += "-fix" }
        clang-tidy @args $file
        return $LASTEXITCODE
    } -ArgumentList $CompileCommandsDir, $headerFilter, [bool]$Fix -ThrottleLimit $Jobs

    $failed = @($results | Where-Object { $_ -ne 0 }).Count
}
else {
    foreach ($file in $tuFiles) {
        Write-Host "clang-tidy: $file" -ForegroundColor Cyan
        clang-tidy @tidyArgsBase $file
        if ($LASTEXITCODE -ne 0) { $failed++ }
    }
}

if ($failed -ne 0) {
    Write-Error "clang-tidy reported issues in $failed file(s)."
    exit 1
}

Write-Host "clang-tidy finished successfully." -ForegroundColor Green

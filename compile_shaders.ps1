#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Compiles GLSL shaders to SPIR-V binary format using glslc
.DESCRIPTION
    Windows PowerShell version of compile_shaders.sh
.PARAMETER InputDir
    Directory containing shader files (default: assets/shaders)
.PARAMETER OutputDir
    Directory for compiled shaders (default: assets/shaders/compiled)
#>

param(
    [string]$InputDir = "assets/shaders",
    [string]$OutputDir = "assets/shaders/compiled"
)

$includeArgs = @(
    "-I", (Resolve-Path $InputDir).Path,
    "-I", (Resolve-Path (Join-Path $InputDir "includes")).Path
)

Write-Host "[Shader Compilation]" -NoNewline
Write-Host " Starting shader compilation..." -ForegroundColor Green

# Create output directory if it doesn't exist
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Compile vertex shaders
$vertexShaders = Get-ChildItem -Path $InputDir -Filter "*.vert" -ErrorAction SilentlyContinue
if ($vertexShaders) {
    foreach ($shader in $vertexShaders) {
        $outputFile = Join-Path $OutputDir "$($shader.BaseName)$($shader.Extension).spv"
        try {
            glslc --target-spv=spv1.5 @includeArgs $shader.FullName -o $outputFile 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Host "Compiling: $($shader.Name) [Compiled] -> $($shader.BaseName).vert.spv" -ForegroundColor Green
            }
            else {
                Write-Host "Compiling: $($shader.Name) [Failed]" -ForegroundColor Red
            }
        }
        catch {
            Write-Host "Compiling: $($shader.Name) [Error] glslc not found. Install Vulkan SDK." -ForegroundColor Red
            exit 1
        }
    }
}

# Compile fragment shaders
$fragmentShaders = Get-ChildItem -Path $InputDir -Filter "*.frag" -ErrorAction SilentlyContinue
if ($fragmentShaders) {
    foreach ($shader in $fragmentShaders) {
        $outputFile = Join-Path $OutputDir "$($shader.BaseName)$($shader.Extension).spv"
        glslc --target-spv=spv1.5 @includeArgs $shader.FullName -o $outputFile 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compiling: $($shader.Name) [Compiled] -> $($shader.BaseName).frag.spv" -ForegroundColor Green
        }
        else {
            Write-Host "Compiling: $($shader.Name) [Failed]" -ForegroundColor Red
        }

        # Build an additional "standard" PBR variant with expensive rarely-used features compiled out.
        # This reduces register pressure/occupancy cost compared to relying on runtime branches.
        if ($shader.Name -ieq "pbr_shader.frag") {
            $standardOut = Join-Path $OutputDir "pbr_shader_standard.frag.spv"
            $standardDefines = @(
                "-DPBR_ENABLE_DEBUG=0",
                "-DPBR_ENABLE_SPEC_GLOSS=0",
                "-DPBR_ENABLE_IRIDESCENCE=0",
                "-DPBR_ENABLE_TRANSMISSION=0",
                "-DPBR_ENABLE_CLEARCOAT=0",
                "-DPBR_ENABLE_ANISOTROPY=0"
            )

            glslc --target-spv=spv1.5 @standardDefines @includeArgs $shader.FullName -o $standardOut 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Host "Compiling: $($shader.Name) [Variant] -> pbr_shader_standard.frag.spv" -ForegroundColor Green
            }
            else {
                Write-Host "Compiling: $($shader.Name) [Variant Failed]" -ForegroundColor Red
            }
        }
    }
}

# Compile compute shaders
$computeShaders = Get-ChildItem -Path $InputDir -Filter "*.comp" -ErrorAction SilentlyContinue
if ($computeShaders) {
    foreach ($shader in $computeShaders) {
        $outputFile = Join-Path $OutputDir "$($shader.BaseName)$($shader.Extension).spv"
        glslc --target-spv=spv1.5 @includeArgs $shader.FullName -o $outputFile 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compiling: $($shader.Name) [Compiled] -> $($shader.BaseName).comp.spv" -ForegroundColor Green
        }
        else {
            Write-Host "Compiling: $($shader.Name) [Failed]" -ForegroundColor Red
        }
    }
}

# Compile geometry shaders
$geometryShaders = Get-ChildItem -Path $InputDir -Filter "*.geom" -ErrorAction SilentlyContinue
if ($geometryShaders) {
    foreach ($shader in $geometryShaders) {
        $outputFile = Join-Path $OutputDir "$($shader.BaseName)$($shader.Extension).spv"
        glslc --target-spv=spv1.5 @includeArgs $shader.FullName -o $outputFile 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compiling: $($shader.Name) [Compiled] -> $($shader.BaseName).geom.spv" -ForegroundColor Green
        }
        else {
            Write-Host "Compiling: $($shader.Name) [Failed]" -ForegroundColor Red
        }
    }
}

# Compile geometry shaders
$geometryShaders = Get-ChildItem -Path $InputDir -Filter "*.mesh" -ErrorAction SilentlyContinue
if ($geometryShaders) {
    foreach ($shader in $geometryShaders) {
        $outputFile = Join-Path $OutputDir "$($shader.BaseName)$($shader.Extension).spv"
        glslc --target-spv=spv1.5 @includeArgs $shader.FullName -o $outputFile 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compiling: $($shader.Name) [Compiled] -> $($shader.BaseName).geom.spv" -ForegroundColor Green
        }
        else {
            Write-Host "Compiling: $($shader.Name) [Failed]" -ForegroundColor Red
        }
    }
}

# Compile geometry shaders
$geometryShaders = Get-ChildItem -Path $InputDir -Filter "*.task" -ErrorAction SilentlyContinue
if ($geometryShaders) {
    foreach ($shader in $geometryShaders) {
        $outputFile = Join-Path $OutputDir "$($shader.BaseName)$($shader.Extension).spv"
        glslc --target-spv=spv1.5 @includeArgs $shader.FullName -o $outputFile 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compiling: $($shader.Name) [Compiled] -> $($shader.BaseName).geom.spv" -ForegroundColor Green
        }
        else {
            Write-Host "Compiling: $($shader.Name) [Failed]" -ForegroundColor Red
        }
    }
}

Write-Host "[Shader Compilation]" -NoNewline
Write-Host " Complete!" -ForegroundColor Green

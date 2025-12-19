# Vulkan Engine - Windows Build Guide

## Quick Start

### 1. Run the Setup Script
```powershell
cd c:\path\to\VulkanEngine
powershell -ExecutionPolicy Bypass -File setup_windows.ps1
```

This will check for all required tools and guide you through what's missing.

### 2. Install Missing Components

The setup script will tell you which of these are needed:

| Component | Purpose | Install From |
|-----------|---------|--------------|
| **C++ Compiler** | Compile code | [Visual Studio 2022](https://visualstudio.microsoft.com) or [Clang](https://llvm.org) or [MinGW](https://www.mingw-w64.org) |
| **Vulkan SDK** | Graphics library | [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) |
| **glslc** | Shader compiler | Included with Vulkan SDK |
| **xmake** | Build system | [xmake.io](https://xmake.io) |
| **clang-format** | Code formatting | `winget install LLVM.LLVM` |

---

## Recommended: Visual Studio 2022

### Installation Steps:

1. **Download Visual Studio 2022 Community**
   - Go to https://visualstudio.microsoft.com/downloads/
   - Click "Community" download

2. **Run the Installer**
   - After download completes, run the installer
   - Click "Continue" when prompted

3. **Select Workload**
   - Look for **"Desktop development with C++"**
   - Check the box next to it
   - On the right side, ensure these are checked:
     - MSVC v143 or later
     - Windows 10/11 SDK
     - CMake tools for Windows

4. **Install**
   - Click "Install"
   - Wait for installation to complete (may take 30+ minutes)
   - Restart your computer when prompted

5. **Install Vulkan SDK**
   - Go to https://vulkan.lunarg.com/sdk/home
   - Download the Windows installer
   - Run the installer and follow prompts
   - Accept default installation location

6. **Verify Installation**
   ```powershell
   cl.exe /?           # MSVC compiler
   glslc --version     # Shader compiler
   ```

### Now Build:

```powershell
cd c:\path\to\VulkanEngine
xmake clean
xmake f -p windows -a x64 -m debug
xmake build Cube
xmake run Cube
```

---

## Alternative: Clang + MinGW

If you don't want to install Visual Studio:

1. **Install LLVM/Clang**
   ```powershell
   winget install LLVM.LLVM
   ```
   - Add to PATH when prompted

2. **Install MinGW-w64**
   - Download from https://www.mingw-w64.org/
   - Choose x86_64 architecture
   - Add `C:\mingw64\bin` to PATH

3. **Install Vulkan SDK**
   - Download from https://vulkan.lunarg.com/sdk/home
   - Run installer with default settings

4. **Build**
   ```powershell
   xmake f -p mingw -a x64 -m debug
   xmake build Cube
   ```

---

## Alternative: WSL2 (Windows Subsystem for Linux)

Use the original Linux build system inside Windows:

```powershell
wsl --install
```

Then inside WSL2:
```bash
sudo apt update
sudo apt install build-essential xmake vulkan-tools libvulkan-dev
cd /mnt/c/path/to/VulkanEngine
xmake f -p linux -a x64 -m debug
xmake build Cube
```

---

## Build Commands

Once setup is complete:

```powershell
# Configure project (only needed once)
xmake f -p windows -a x64 -m debug

# Build the Cube demo
xmake build Cube

# Run the demo
xmake run Cube

# Clean build artifacts
xmake clean

# Rebuild everything
xmake f -c       # Clear config
xmake f -p windows -a x64 -m debug
xmake build -r Cube
```

---

## Troubleshooting

### "Microsoft Visual Studio not found"
```
Solution: Install Visual Studio 2022 with C++ workload, OR use clang/mingw instead
Command: xmake f -p mingw -a x64 -m debug
```

### "Vulkan package not found"
```
Solution: Install Vulkan SDK from https://vulkan.lunarg.com/sdk/home
Verify: glslc --version
```

### "glslc not found"
```
Solution: Install Vulkan SDK or install glslc separately
Verify: glslc --version
```

### "clang-format: command not found" (optional warning)
```
Solution: Install LLVM/Clang
Command: winget install LLVM.LLVM
Note: This is optional - formatting is skipped if not found
```

### Build fails with cryptic errors
```
Try: xmake f -c          # Clear configuration cache
Then: xmake f -p windows -a x64 -m debug
Then: xmake build Cube
```

### "cannot get program for cxx"
```
Solution: C++ compiler not properly installed
Try:
  - Check if cl.exe or clang++ is in PATH
  - Reinstall Visual Studio or Clang
  - Restart your terminal
```

---

## Environment Variables

### Optional but recommended:

```powershell
# Set Vulkan SDK path
[Environment]::SetEnvironmentVariable("VULKAN_SDK", "C:\VulkanSDK\1.x.x.0", "User")

# Set MinGW path (if using MinGW)
[Environment]::SetEnvironmentVariable("MINGW_HOME", "C:\mingw64", "User")
```

Then restart PowerShell for changes to take effect.

---

## Project Structure

```
VulkanEngine/
├── src/
│   ├── Engine/          # Core engine library
│   └── demos/Cube/      # Cube demo application
├── include/             # Header files
├── assets/
│   ├── shaders/         # GLSL shader source files
│   ├── shaders/compiled/# Compiled SPIR-V shaders
│   ├── textures/        # Texture files
│   └── models/          # 3D model files
├── xmake.lua            # Build configuration
├── format_code.ps1      # Code formatter (Windows)
├── compile_shaders.ps1  # Shader compiler (Windows)
└── WINDOWS_BUILD_SETUP.md
```

---

## Cross-Platform Support

This project now supports:
- ✅ **Linux** (gcc, clang, native Vulkan)
- ✅ **Windows** (MSVC, Clang, MinGW with Volk)
- ✅ **WSL2** (Linux toolchain on Windows)

Platform-specific features:
- Shader compilation works on all platforms
- Code formatting works on all platforms
- Vulkan detection automatic (uses volk on Windows for portability)

---

## Performance Tips

| Compiler | Speed | Size | Compatibility |
|----------|-------|------|---|
| MSVC | Fast | Medium | Windows only |
| Clang | Fast | Medium | All platforms |
| GCC/MinGW | Medium | Medium | All platforms |

**Recommended**: MSVC (Visual Studio) for Windows development

---

## Getting Help

1. Check [WINDOWS_BUILD_SETUP.md](WINDOWS_BUILD_SETUP.md) for detailed setup instructions
2. Run `setup_windows.ps1` to diagnose issues
3. Check xmake logs: `C:\Users\<username>\AppData\Local\.xmake\cache\packages\`
4. For Vulkan issues: https://vulkan.lunarg.com/
5. For xmake issues: https://xmake.io/guide/

---

## Related Files

- [WINDOWS_BUILD_SETUP.md](WINDOWS_BUILD_SETUP.md) - Detailed platform setup guide
- [xmake.lua](xmake.lua) - Build configuration (platform-aware)
- [compile_shaders.ps1](compile_shaders.ps1) - PowerShell shader compiler
- [format_code.ps1](format_code.ps1) - PowerShell code formatter
- [setup_windows.ps1](setup_windows.ps1) - Windows environment checker

---

## Next Steps

1. Run the setup script
2. Install any missing components
3. Follow the "Quick Start" section above
4. Check out the [main README](readme.md) for project information

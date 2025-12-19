# Windows Build Setup Guide for Vulkan Engine

## Current Issues

Your project works on Linux but requires additional setup for Windows. The main issues are:

1. **No C++ Compiler**: Visual Studio, MinGW, or Clang needs to be installed
2. **Vulkan SDK**: Needs to be installed separately
3. **Build Tool**: xmake needs a compatible toolchain

## Solution: Choose ONE of these approaches

### Option 1: Visual Studio 2022 (Recommended)

This is the most straightforward approach.

#### Steps:
1. Download Visual Studio 2022 Community from: https://visualstudio.microsoft.com/downloads/
2. Run the installer
3. Select workload: **Desktop development with C++**
4. Install the following components:
   - MSVC v143 or later
   - Windows 10/11 SDK
   - CMake tools for Windows

5. Download and install Vulkan SDK from: https://vulkan.lunarg.com/sdk/home
   - Choose the latest version
   - Install to default location (will be auto-detected)

6. Install Shaderc (for shader compilation):
   ```powershell
   # From the Vulkan SDK bin directory, or via vcpkg:
   vcpkg install shaderc:x64-windows
   ```

7. Then configure and build:
   ```powershell
   cd c:\path\to\VulkanEngine
   xmake f -p windows -a x64 -m debug
   xmake build Cube
   ```

---

### Option 2: MinGW-w64 + Clang (Alternative)

#### Steps:
1. Install LLVM/Clang from: https://github.com/llvm/llvm-project/releases
   - Choose the Windows installer
   - During installation, add to PATH

2. Install MinGW-w64 from: https://www.mingw-w64.org/
   - Choose x86_64 architecture
   - Install to a simple path like `C:\mingw64`
   - Add `C:\mingw64\bin` to PATH

3. Install Vulkan SDK: https://vulkan.lunarg.com/sdk/home

4. Install glslc (shader compiler):
   - Included in Vulkan SDK at: `VulkanSDK\Bin\glslc.exe`
   - Ensure it's in PATH

5. Configure and build:
   ```powershell
   xmake f -p mingw -a x64 -m debug
   xmake build Cube
   ```

---

### Option 3: WSL2 (Windows Subsystem for Linux)

If you prefer to keep using the Linux build system:

1. Install WSL2: https://docs.microsoft.com/en-us/windows/wsl/install
2. Install Ubuntu 22.04 in WSL2
3. Inside WSL2, run the Linux build commands:
   ```bash
   sudo apt-get install build-essential cmake git
   sudo apt-get install vulkan-tools libvulkan-dev
   sudo apt-get install xmake
   xmake f -p linux -a x64 -m debug
   xmake build Cube
   ```

---

## Troubleshooting

### "checking for Microsoft Visual Studio (x64) version ... no"
- Visual Studio C++ tools are not installed
- Solution: Install Visual Studio with C++ workload OR choose Option 2/3

### "Vulkan package not found"
- Vulkan SDK not installed or not in PATH
- Solution: Install Vulkan SDK and ensure VULKAN_SDK environment variable is set

### "glslc not found"
- Shader compiler missing
- Solution: Install Vulkan SDK or shaderc separately and add to PATH

### Build fails with "toolchain not found"
- xmake cannot find a C++ compiler
- Solution: Install a compiler toolchain (Visual Studio, MinGW, or Clang)

---

## Additional Requirements

### All platforms need:
- **glslc** (shader compiler) - installed with Vulkan SDK
- **clang-format** - for code formatting
  ```powershell
  winget install LLVM.LLVM
  ```

### Environment Variables to set (optional but helpful):

For Visual Studio:
```powershell
[Environment]::SetEnvironmentVariable("VULKAN_SDK", "C:\VulkanSDK\1.x.x.0", "User")
```

For MinGW:
```powershell
[Environment]::SetEnvironmentVariable("MINGW_HOME", "C:\mingw64", "User")
[Environment]::SetEnvironmentVariable("VULKAN_SDK", "C:\VulkanSDK\1.x.x.0", "User")
```

---

## Next Steps

1. Choose and install ONE of the three options above
2. Verify installations:
   ```powershell
   # Check C++ compiler
   cl.exe --version        # For MSVC
   clang++ --version       # For Clang
   
   # Check shader compiler
   glslc --version
   
   # Check build system
   xmake --version
   ```

3. Run the build:
   ```powershell
   cd c:\path\to\VulkanEngine
   xmake clean
   xmake f -c
   xmake f -p windows -a x64 -m debug
   xmake build Cube
   xmake run Cube
   ```

---

## Project Files Updated for Windows Support

The following changes have been made to support Windows:

- **xmake.lua**: Updated to handle platform-specific configs
  - Uses `volk` instead of `vulkan` on Windows (more portable)
  - Platform-specific shader compilation
  - Platform-specific code formatting

- **build_shaders.ps1** / **format_code.ps1**: New PowerShell scripts for Windows

These changes maintain Linux compatibility while adding Windows support.

---

## Performance Notes

- **Visual Studio**: Best performance, most comprehensive tooling
- **Clang/MinGW**: Slightly slower builds but smaller install size
- **WSL2**: Compatible with existing Linux build system but slightly slower than native Windows

Recommended: **Option 1 (Visual Studio)** for best experience on Windows

# Architecture Restructuring Plan

## Current State Analysis
- **Cleanup**: `2dEngine` and `ThreeBodiesSimulation` have been moved out, leaving `3dEngine` as the sole focus.
- **Flat Structure**: `3dEngine` has many files in a single directory, mixing low-level Vulkan wrappers with high-level Scene objects.
- **Naming**: `3dEngine` is a generic name. We will rename it to `Engine` and structure it properly.

## Proposed Structure (Refactoring `3dEngine` -> `Engine`)

We will restructure the existing `3dEngine` into a modular library.

```text
include/
  Engine/
    Core/           # Foundation (Window, Input, Logger, Exceptions, Utils)
    Graphics/       # Low-level Rendering (Device, SwapChain, Pipeline, Buffers, Descriptors, Renderer)
    Scene/          # High-level Scene Graph (GameObject, Camera)
    Resources/      # Asset Management (Model, Texture, Materials)
    Systems/        # Render Systems (MeshRenderSystem, etc.)
src/
  Engine/
    Core/
    Graphics/
    Scene/
    Resources/
    Systems/
```

## Migration Steps

1.  **Cleanup**: Remove leftover `include/2dEngine` if present.
2.  **Create Directories**: Set up the new `Engine` folder structure in `include` and `src`.
3.  **Move Core Components**: Move `Window`, `Keyboard`, `Mouse`, `Exceptions`, `utils` to `Engine/Core`.
4.  **Move Graphics Components**: Move `Device`, `DeviceMemory`, `SwapChain`, `Pipeline`, `Buffer`, `Descriptors`, `Renderer`, `FrameInfo` to `Engine/Graphics`.
5.  **Move Resource Components**: Move `Model`, `Texture`, `PBRMaterial` to `Engine/Resources`.
6.  **Move Scene Components**: Move `GameObject`, `Camera` to `Engine/Scene`.
7.  **Move Systems**: Move `*System` classes to `Engine/Systems`.
8.  **Update Includes**: Update all `#include` directives in the project (e.g., `#include "3dEngine/Device.hpp"` -> `#include "Engine/Graphics/Device.hpp"`).
9.  **Update Build System**: Modify `xmake.lua` to build the new `Engine` target.

## Questions
1.  Shall I proceed with removing `include/2dEngine` and restructuring `3dEngine` as described?

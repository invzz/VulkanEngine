# Copilot Instructions for VulkanEngine

## Project Overview
A modern Vulkan 1.3+ rendering engine with deferred PBR rendering, mesh shaders, ECS architecture (EnTT), and a render graph system. Built with xmake (C++20).

## Architecture

### Core Libraries (dependency order)
1. **EngineSceneIO** - Scene serialization (JSON), light components
2. **ModelLib** - Model/texture loading (glTF via tinygltf), mesh optimization (meshoptimizer)
3. **Engine** - Graphics core, systems, rendering

### Module Structure (`include/Engine/` mirrors `src/Engine/`)
- `Core/` - Window (GLFW), input, exceptions
- `Graphics/` - Vulkan abstractions: `Device`, `SwapChain`, `Pipeline`, `Descriptors`, `RenderGraph`
- `Scene/` - ECS Scene wrapper, Camera, components (`components/` subfolder)
- `Systems/` - Render systems: `ModelRenderSystem`, `ShadowSystem`, `IBLSystem`, `DeferredLightingSystem`

### Key Patterns

**ECS Usage (EnTT)**
```cpp
auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
for (auto entity : view) {
    auto& model = view.get<ModelComponent>(entity);
    // ...
}
```

**Descriptor Builder Pattern** - Always use builders for Vulkan descriptors:
```cpp
auto pool = DescriptorPool::Builder(device)
    .setMaxSets(maxSets)
    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets)
    .build();
```

**RenderGraph** - Passes are `RenderPass` subclasses or `LambdaRenderPass` lambdas added via `addPass()`.

**Push Constants** - Must match GLSL exactly. See `MeshPushConstantData` in [ModelRenderSystem.hpp](include/Engine/Systems/ModelRenderSystem.hpp) with `static_assert` offset checks.

## Build & Run

```powershell
# Windows (requires VULKAN_SDK env var set)
xmake f -m debug    # or -m release
xmake
xmake run Editor    # Main editor application
xmake run Tests     # Run test suite
```

Shaders auto-compile during build. Manual recompile: `./compile_shaders.ps1`

## Shaders

- Source: `assets/shaders/` (GLSL 450, SPIR-V 1.5)
- Compiled output: `assets/shaders/compiled/*.spv`
- Shared includes: `assets/shaders/includes/` (`common.glsl`, `brdf.glsl`, `shadows.glsl`, etc.)
- Use `#include "includes/..."` with `GL_GOOGLE_include_directive`

**PBR Variants**: `pbr_shader.frag` compiles standard variant with feature toggles:
```glsl
#ifndef PBR_ENABLE_TRANSMISSION
#define PBR_ENABLE_TRANSMISSION 1
#endif
```

## Testing

Tests use Google Test. Key test patterns:
- Descriptor tests: `tests/descriptor_*.cpp` - Pool allocation, overflow, fragmentation
- Scene tests: `tests/scene_serializer_tests.cpp` - Roundtrip serialization
- Thread tests: `tests/thread_local_command_pool_tests.cpp`

All GPU tests require a `Device` instance (creates minimal window):
```cpp
Window win(16, 16, "Test");
Device device(win);
```

## Scene Format

Scenes are JSON files in `assets/scenes/`. Schema in [docs/scene_schema.md](docs/scene_schema.md).

Key fields: `objects` (mesh instances with transforms), `lights` (directional/point/spot with bake settings).

## Conventions

- Initialize all Vulkan handles to `VK_NULL_HANDLE`
- RAII for Vulkan resources; destructors must null-check and not throw
- Use `Device::beginSingleTimeCommands()` / `endSingleTimeCommands()` for one-shot GPU work
- Thread-local command pools available via `allocateSecondaryCommandBuffer()`
- Double-buffering: `Device::kMaxFramesInFlight = 2`

## Active Refactoring (see REFACTOR_PLAN.md)

The Device/Graphics stack is being incrementally improved:
- Centralizing extension helpers in `ExtensionHelpers.hpp`
- Migrating to consistent RAII wrappers
- Standardizing descriptor pool usage across systems

## Tools

- **Editor** - Main application with ImGui UI
- **IBLBaker** - Environment map preprocessing tool
- **Shaders** target - Phony target to recompile all shaders

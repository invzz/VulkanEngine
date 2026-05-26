# Vulkan Engine

A modern Vulkan-based 3D rendering engine built with C++20, featuring a data-oriented architecture powered by the EnTT Entity Component System (ECS), a flexible Render Graph for managing rendering pipelines, and support for advanced rendering techniques including Physically Based Rendering (PBR) and Image Based Lighting (IBL).

## Features

### Rendering
- **Vulkan Backend** - Low-level GPU control for maximum performance
- **Physically Based Rendering (PBR)** - Realistic lighting and material models
- **Image Based Lighting (IBL)** - Environment lighting for reflections and ambient occlusion
- **Shadow Mapping** - Directional, Point, Spot, and Cube shadow maps with CPU culling
- **Deferred Lighting** - Forward+ style deferred lighting system
- **Post-Processing** - Integrated post-processing pipeline
- **Skybox** - Procedural and cubemap skybox support
- **Volumetric Dust** - Dust particle rendering system
- **Morph Targets** - Compute shader-based morph target animation
- **LOD System** - Level of Detail management for performance optimization
- **Grid Rendering** - Debug/world grid visualization

### Architecture
- **EngineState** - Centralized runtime state as single source of truth
- **Render Graph** - Dependency-driven graph for managing render passes and resources
- **Entity Component System (ECS)** - EnTT-powered data-oriented design
- **System Registry** - Type-safe system registration and lifecycle management
- **Multi-threading** - Parallel command buffer recording across work queues
- **GPU Profiler** - Vulkan query-based GPU timing and profiling
- **Shader Hot-Reload** - Shader variant management with file change detection

### Scene & Assets
- **glTF 2.0 Support** - Full scene and model loading via tinygltf
- **Mesh Optimization** - Vertex cache optimization and index compression via meshoptimizer
- **Texture Management** - Automatic loading and caching (PNG, HDR/EXR)
- **Camera System** - Perspective and orthographic camera with transform management
- **Animation System** - glTF animation playback

### Editor
- **ImGui-based UI** - Debug panels, scene hierarchy, property inspectors
- **Scene Loading** - Load and visualize glTF scenes in real-time
- **IBL Baker** - Standalone tool for precomputing environment maps (irradiance, prefilter, BRDF LUT)
- **Scene Exporter** - Export scene data for game engines

## Quick Start

### Prerequisites

- A Vulkan-capable GPU and drivers
- [xmake](https://xmake.io/) build system
- [Vulkan SDK](https://vulkan.lunarg.com/) (provides `glslc` for shader compilation)
- Development libraries (auto-fetched by xmake):
  - GLFW, GLM, EnTT, ImGui, TinyGLTF, TinyEXR, nlohmann_json, meshoptimizer, stb, gtest

### Build

```bash
# Debug build (with Vulkan validation layers enabled)
xmake f -m debug
xmake

# Release build
xmake f -m release
xmake
```

### Run

```bash
# Editor
xmake run Editor

# Cube demo
xmake run Cube

# Tests
xmake run Tests

# IBL Baker tool
xmake run IBLBaker
```

### Validation Control

| Mode | Validation Default |
|------|-------------------|
| Debug | Enabled |
| Release | Disabled |

Force validation in any mode:
```bash
xmake f -m release --validation=y
xmake
```

Runtime override (at launch):
```bash
xmake run Editor -- --validation=off
```

## Project Structure

```
VulkanEngine/
├── include/Engine/           # Public headers
│   ├── Core/                 # Window, input, logging, utilities
│   ├── Graphics/             # Vulkan abstractions (Device, SwapChain, Pipeline, etc.)
│   │   ├── FrameGraph/       # Render graph implementation
│   │   └── Passes/           # Render pass headers (GBuffer, Shadow, etc.)
│   ├── Scene/                # Camera, components, scene, skybox
│   └── Systems/              # ECS systems (PBR, lighting, shadows, etc.)
├── src/
│   ├── Editor/               # Main editor application + ImGui panels
│   │   └── ui/               # Debug panels (animation, camera, IBL, etc.)
│   ├── Engine/               # Engine implementation
│   │   ├── Core/
│   │   ├── Graphics/
│   │   ├── Scene/
│   │   └── Systems/
│   ├── EngineSceneIO/        # Scene import/export logic
│   ├── ModelLib/             # Model loading (glTF, obj, HDR textures)
│   ├── demos/                # Standalone demos
│   └── tools/
│       ├── IBLBaker/         # IBL precomputation tool
│       └── SceneExporter/    # Scene export tool
├── assets/
│   ├── shaders/              # GLSL source + compiled SPV binaries
│   │   ├── includes/         # Shared GLSL include files
│   │   └── compiled/         # Compiled SPIR-V shaders
│   ├── models/               # glTF test models
│   ├── textures/             # Texture assets
│   └── scenes/               # Scene files
├── tests/                    # Google Test test suite
│   ├── Engine/
│   ├── EngineSceneIO/
│   ├── ModelLib/
│   └── Benchmarks/
├── xmake.lua                 # Build configuration
├── compile_shaders.sh/ps1    # Shader compilation scripts
└── format_code.sh/ps1        # Code formatting scripts
```

## Rendering Pipeline

The engine uses a Render Graph to orchestrate the following passes:

1. **Depth Prepass** - Opaque geometry depth-only rendering
2. **G-Buffer Pass** - Geometry information (position, normal, albedo, roughness/metallic)
3. **Shadow Pass** - Directional/point/spot/cube shadow maps
4. **Deferred Lighting** - Per-light shading with PCF/PCSS filtering
5. **Post-Processing** - Final composition and effects
6. **Composition** - ImGui overlay and presentation

## Shader Compilation

Shaders are compiled automatically during the build. To manually regenerate:

```bash
./compile_shaders.sh   # Linux
.\compile_shaders.ps1  # Windows
```

## Testing

```bash
# Run all tests
xmake run Tests

# Run specific test suite
xmake run Tests -- --gtest_filter=Engine.*

# Run specific test
xmake run Tests -- --gtest_filter=ModelRenderSystemTests.*
```

## Code Quality

```bash
# Format code
./format_code.sh
```

## Platform Support

| Platform | Status |
|----------|--------|
| Linux (Wayland) | Supported (default) |
| Windows (DXGI) | Supported |

## Dependencies

| Library | Purpose |
|---------|---------|
| [GLFW](https://github.com/glfw/glfw) | Window management and input |
| [GLM](https://github.com/g-truc/glm) | Mathematics (vectors, matrices) |
| [EnTT](https://github.com/medik/deps/tree/main/entt) | Entity Component System (ECS) |
| [ImGui](https://github.com/ocornut/imgui) | Immediate mode GUI (debug editor) |
| [TinyGLTF](https://github.com/syoyo/tinygltf) | glTF 2.0 model/scene loading |
| [TinyOBJLoader](https://github.com/tinyobjloader/tinyobjloader) | OBJ model loading |
| [TinyEXR](https://github.com/syoyo/tinyexr) | HDR/EXR texture loading |
| [stb](https://github.com/nothings/stb) | LDR (PNG, JPG) texture loading |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON serialization |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | Mesh optimization |
| [Google Test](https://github.com/google/googletest) | Unit testing framework |

## License

This project is available under the [LICENSE](./LICENSE) file.

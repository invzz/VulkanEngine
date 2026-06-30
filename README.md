# VulkanEngine

A modern Vulkan-based 3D rendering engine and editor built with C++20, powered by the EnTT Entity Component System (ECS) and a flexible Render Graph architecture.

## Features

### Rendering
- **Vulkan 1.4 Backend** — Low-level GPU control with dynamic rendering and shader objects
- **Physically Based Rendering (PBR)** — Cook-Torrance microfacet BRDF with metal/roughness workflow
- **Image Based Lighting (IBL)** — Diffuse irradiance, specular prefilter, and BRDF LUT from cubemaps
- **Deferred Lighting** — G-buffer (normal/albedo/material/emissive) + per-light shading
- **Shadow Mapping** — Directional cascaded, point (cube), and spot shadow maps with PCF
- **Transparency** — Transmission, alpha-blend, and refraction via scene-color copy + mip blur
- **Morph Targets** — Compute-shader blend shape animation
- **Animation System** — glTF skeletal animation playback
- **Mesh Shading** — Meshlet-based culling and rendering pipeline
- **LOD System** — Distance-based level-of-detail selection
- **GPU Profiler** — Vulkan query-based per-pass timing
- **Shader Hot-Reload** — File-watch driven variant recompilation

### Editor
- **ImGui Docking** — Multi-panel workspace with customizable layout
- **In-Viewport Gizmos** — Translate / Rotate / Scale via ImGuizmo (World/Local space)
- **Per-Triangle Object Picking** — CPU ray-triangle intersection using collision geometry
- **Selection Outline** — Selected object highlighted in viewport
- **Scene Hierarchy** — Entity tree with drag-select
- **Inspector Panel** — Transform (T/R/S), light, and animation editing
- **Physics Debug** — Jolt Physics collider wireframe overlay
- **Scene Persistence** — Save/load via JSON serialization

### Architecture
- **Render Graph** — Explicit dependency-driven pass graph; each pass declares inputs and outputs
- **Entity Component System** — EnTT-powered data-oriented entity management
- **System Registry** — Type-safe system registration and lifecycle
- **Multi-threaded Recording** — Parallel command buffer recording across work queues
- **Dependency Injection** — Explicit constructor-injected dependencies (no service locator)

## Quick Start

### Prerequisites

- A Vulkan-capable GPU with up-to-date drivers
- [xmake](https://xmake.io/) build system
- [Vulkan SDK](https://vulkan.lunarg.com/) (provides `glslc` for shader compilation)

### Build

```bash
# Debug build (Vulkan validation layers enabled)
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
xmake build
```

## Project Structure

```
VulkanEngine/
├── include/
│   └── Engine/           # Public headers
│       ├── Core/         # Window, input, logging
│       ├── Graphics/     # Vulkan wrappers (Device, SwapChain, RenderGraph, Passes)
│       ├── Scene/        # Camera, ECS components, Scene
│       └── Systems/      # Rendering and simulation systems
├── src/
│   ├── Editor/           # Editor application + ImGui panels
│   │   └── ui/           # Viewport, Inspector, Scene, Toolbar, etc.
│   ├── Engine/           # Engine implementation
│   ├── EngineSceneIO/    # Scene import/export
│   ├── ModelLib/         # Model loading (glTF, OBJ, textures)
│   ├── third_party/      # Vendored libraries
│   │   ├── stb/
│   │   └── ImGuizmo/
│   └── tools/
│       └── IBLBaker/     # Environment map precomputation
├── assets/
│   ├── shaders/          # GLSL source + compiled SPIR-V
│   ├── models/           # glTF test models
│   ├── textures/         # Texture assets
│   └── scenes/           # Serialized scene files
├── tests/                # Google Test suite
│   ├── Engine/
│   ├── EngineSceneIO/
│   ├── ModelLib/
│   └── Benchmarks/
├── xmake.lua             # Build configuration
├── .clang-format         # Code style configuration
├── compile_shaders.py    # Shader compilation
└── scripts/
    └── format_code.py    # Code formatting
```

## Rendering Pipeline

The Render Graph orchestrates these passes in order:

1. **Update** — Input, physics, camera, selection, animation
2. **Compute** — Morph target blending
3. **Shadow** — Directional / point / spot shadow maps
4. **Depth Prepass** — Opaque depth-only (for Hi-Z)
5. **G-Buffer** — Normal, albedo, material, emissive (with depth prepass load)
6. **Deferred Lighting** — Per-pixel shading against G-buffer
7. **Forward** — Transmission, alpha-blend, grid, collider debug, selection outline, mipmap generation
8. **TransitionToReadOnly** — Offscreen color → `SHADER_READ_ONLY_OPTIMAL` for ImGui sampling
9. **Composition** — ImGui overlay + swapchain presentation

## Viewport & Picking

- **Navigation mode** — WASD + mouse-look camera control (right-click or toolbar toggle)
- **Picking mode** — Click on objects in the viewport to select them
  - Models: ray-triangle intersection against CPU collision geometry (per-triangle, not AABB)
  - Cameras / Lights: screen-space radius test from projected position
  - Gizmo-aware: picking is suppressed while the gizmo is being dragged or hovered
- **Gizmo** — T/R/S buttons in the toolbar, World/Local toggle

## Dependencies

| Library | Purpose |
|---------|---------|
| [GLFW](https://github.com/glfw/glfw) | Window management, input, Vulkan surface |
| [GLM](https://github.com/g-truc/glm) | Mathematics (vectors, matrices, quaternions) |
| [EnTT](https://github.com/medik/deps/tree/main/entt) | Entity Component System |
| [ImGui](https://github.com/ocornut/imgui) | Immediate-mode GUI (docking branch) |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | 3D transformation gizmo overlay |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | Multi-core rigid-body physics |
| [TinyGLTF](https://github.com/syoyo/tinygltf) | glTF 2.0 model/scene loading |
| [TinyOBJLoader](https://github.com/tinyobjloader/tinyobjloader) | OBJ model loading |
| [TinyEXR](https://github.com/syoyo/tinyexr) | OpenEXR / HDR texture loading |
| [stb](https://github.com/nothings/stb) | LDR texture loading (PNG, JPEG) |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON serialization |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | Vertex cache optimization, meshlets |
| [Google Test](https://github.com/google/googletest) | Unit testing |

## Platform Support

| Platform | Status |
|----------|--------|
| Linux (Wayland) | Supported (default) |
| Linux (X11) | Supported |
| Windows | Supported |

## License

MIT — see [LICENSE](./LICENSE). This project includes [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) (MIT, © Cedric Guillemet) as a vendored third-party dependency.
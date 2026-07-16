# VulkanEngine

A modern Vulkan-based 3D rendering engine and editor built with C++20, powered by the EnTT Entity Component System (ECS) and a flexible Render Graph architecture.

## Feature Overview

### Rendering

- **Vulkan 1.4 Backend** -- Low-level GPU control with dynamic rendering and shader objects
- **Physically Based Rendering (PBR)** -- Cook-Torrance microfacet BRDF with metal/roughness workflow
- **Image Based Lighting (IBL)** -- Diffuse irradiance, specular prefilter, and BRDF LUT from cubemaps; the procedural sky is baked to a cubemap each time the sun/atmosphere changes and fed to the IBL system automatically
- **Deferred Lighting** -- G-buffer (normal/albedo/material/emissive) + per-light shading
- **Shadow Mapping** -- Directional cascaded, point (cube), and spot shadow maps with PCF
- **Ray-Traced Shadows** -- Hardware ray-query shadows for directional, point, and spot lights with soft penumbra and transparent-object shadowing (optional, per-light-type; falls back to shadow maps when disabled)
- **Transparency** -- Transmission, alpha-blend, and refraction via scene-color copy + mip blur
- **Morph Targets** -- Compute-shader blend shape animation
- **Animation System** -- glTF skeletal animation playback
- **Mesh Shading** -- Meshlet-based culling and rendering pipeline
- **LOD System** -- Distance-based level-of-detail selection
- **GPU Profiler** -- Vulkan query-based per-pass timing
- **Shader Hot-Reload** -- File-watch driven variant recompilation

### Editor

- **ImGui Docking** -- Multi-panel workspace with customizable layout and theme support
- **In-Viewport Gizmos** -- Translate / Rotate / Scale via ImGuizmo (World/Local space)
- **View Orientation Gizmo** -- Corner view gizmo with Orbit Selected / Look In Place behavior
- **Per-Triangle Object Picking** -- CPU ray-triangle intersection using collision geometry
- **Selection Outline** -- Selected object highlighted in viewport
- **Scene Hierarchy** -- Entity tree with set-active-camera and delete actions
- **Inspector Panel** -- Transform (T/R/S), light, and animation editing
- **Physics Debug** -- Jolt Physics collider wireframe overlay
- **Scene Persistence** -- Save/load via JSON serialization

### Architecture

- **Render Graph** -- Explicit dependency-driven pass graph; each pass declares inputs and outputs
- **Entity Component System** -- EnTT-powered data-oriented entity management
- **System Registry** -- Type-safe system registration and lifecycle
- **Multi-threaded Recording** -- Parallel command buffer recording across work queues
- **Dependency Injection** -- Explicit constructor-injected dependencies

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

| Mode    | Validation Default |
|---------|-------------------|
| Debug   | Enabled           |
| Release | Disabled          |

Force validation in any mode:

```bash
xmake f -m release --validation=y
xmake build
```

## Project Structure

```
VulkanEngine/
├── include/Engine/                  # Public headers
│   ├── Core/                        # Window, input, logging, error codes
│   ├── Graphics/                    # Vulkan wrappers
│   │   ├── Passes/                  # RenderGraph pass definitions
│   │   ├── FrameGraph/              # RenderGraph core
│   │   ├── IBL/                     # IBL helper headers
│   │   ├── Device.hpp               # VkDevice, physical device
│   │   ├── AccelBuilder.hpp         # BLAS/TLAS acceleration structures (ray-query shadows)
│   │   ├── SwapChain.hpp            # Swapchain management
│   │   ├── Pipeline.hpp             # Pipeline creation
│   │   ├── Renderer.hpp             # Main renderer
│   │   ├── RenderPipeline.hpp       # RenderGraph pipeline
│   │   ├── RenderTarget.hpp         # Render targets / framebuffers
│   │   ├── Descriptors.hpp          # Descriptor layout / sets
│   │   ├── Buffer.hpp               # Vulkan buffers
│   │   ├── GpuProfiler.hpp          # Query-based GPU timing
│   │   └── ShaderMonitor.hpp        # Hot-reload file watcher
│   ├── Scene/                       # ECS components, camera, scene
│   │   ├── components/              # EnTT ECS data (transform, camera, lights, model, animation)
│   │   ├── Animation/              # Animation clip / controller / graph domain types
│   │   └── Skybox.hpp             # Skybox + procedural sky capture
│   ├── Systems/                     # ECS systems
│   │   ├── IBL/                     # IBL system implementations
│   │   ├── AnimationSystem.hpp
│   │   ├── CameraSystem.hpp
│   │   ├── DeferredLightingSystem.hpp
│   │   ├── IBLSystem.hpp
│   │   ├── InputSystem.hpp
│   │   ├── JoltPhysicsSystem.hpp
│   │   ├── LightSystem.hpp
│   │   ├── LODSystem.hpp
│   │   ├── ModelRenderSystem.hpp
│   │   ├── MorphTargetSystem.hpp
│   │   ├── PickingSystem.hpp
│   │   ├── PostProcessingSystem.hpp
│   │   ├── ShadowSystem.hpp
│   │   └── SkyboxRenderSystem.hpp
│   ├── EngineState.hpp              # Engine runtime state
│   ├── EditorState.hpp              # Editor runtime state
│   └── SystemRegistry.hpp           # Type-safe system registry
├── src/
│   ├── Engine/                      # Engine implementation
│   │   ├── Core/                    # Window, input, logging
│   │   ├── Graphics/                # Vulkan backend (38 .cpp files)
│   │   ├── Scene/                   # Camera, scene utilities, skybox
│   │   └── Systems/                 # ECS system implementations (27 .cpp files)
│   ├── Editor/                      # Editor application
│   │   ├── app.cpp                  # Entry point
│   │   ├── ui/                      # Main UI manager
│   │   │   ├── UIManager.cpp        # Window/panel creation
│   │   │   ├── Panels/              # Editor panels
│   │   │   │   ├── ViewportPanel.cpp
│   │   │   │   ├── ViewportToolbar.cpp
│   │   │   │   ├── ViewportObjectGizmo.cpp
│   │   │   │   ├── ViewportViewGizmo.cpp
│   │   │   │   ├── ScenePanel.cpp
│   │   │   │   ├── InspectorPanel.cpp
│   │   │   │   ├── TransformPanel.cpp
│   │   │   │   ├── CameraPanel.cpp
│   │   │   │   ├── LightsPanel.cpp
│   │   │   │   ├── AnimationPanel.cpp
│   │   │   │   ├── PhysicsPanel.cpp
│   │   │   │   ├── PostProcessPanel.cpp
│   │   │   │   ├── DebugPanel.cpp
│   │   │   │   ├── IBLPanel.cpp
│   │   │   │   ├── SettingsPanel.cpp
│   │   │   │   └── ToolbarPanel.cpp
│   │   │   └── Workspace/           # Layout, themes, docking
│   │   │       ├── LayoutBuilder.cpp
│   │   │       ├── WorkspaceManager.cpp
│   │   │       ├── ThemeSystem.cpp
│   │   │       └── ThemeLoader.cpp
│   │   └── Utils/                   # Icon fonts
│   ├── EngineSceneIO/               # Scene import/export (JSON)
│   ├── ModelLib/                    # Model loading (glTF, OBJ, textures)
│   ├── third_party/                 # Vendored libraries
│   │   ├── stb/                     # Image loading
│   │   ├── ImGuizmo/                # 3D gizmo overlay
│   │   └── ImViewGuizmo/            # View gizmo overlay
│   └── tools/IBLBaker/              # IBL environment map baker
├── assets/
│   ├── shaders/                     # GLSL source + compiled SPIR-V
│   │   ├── includes/                # Shared GLSL include files
│   │   └── compiled/                # Pre-compiled SPIR-V shaders
│   ├── models/                      # glTF test models
│   ├── textures/                    # Texture assets
│   ├── scenes/                      # Serialized scene files
│   ├── fonts/                       # Editor fonts
│   └── editor/                      # Editor themes (JSON)
├── tests/                           # Google Test suite
├── xmake.lua                        # Build configuration
├── compile_shaders.py               # Shader compilation script
├── format_code.py                   # Code formatting script
└── .clang-format                    # Code style
```

## Rendering Pipeline

The Render Graph orchestrates these passes in order:

1. **Update** -- Input, physics, camera, selection, animation
2. **Compute** -- Morph target blending
3. **Shadow** -- Directional / point / spot shadow maps
4. **Depth Prepass** -- Opaque depth-only (for Hi-Z)
5. **G-Buffer** -- Normal, albedo, material, emissive (with depth prepass load)
6. **Deferred Lighting** -- Per-pixel shading against G-buffer
7. **Forward** -- Transmission, alpha-blend, grid, collider debug, selection outline, mipmap generation
8. **Selection Mask** -- Render selected geometry into a 1-channel silhouette mask
9. **TransitionToReadOnly** -- Offscreen color -> `SHADER_READ_ONLY_OPTIMAL` for ImGui sampling
10. **Post-Process** -- Tonemap, SSAO, bloom, exposure
11. **Selection Composite** -- Edge-detect on the mask, blended over the post-fx image
12. **Composition** -- ImGui overlay + swapchain presentation

## Procedural Sky & IBL

The renderer can drive Image Based Lighting directly from the procedural atmosphere model, so surfaces receive diffuse (irradiance) and specular (prefiltered env + BRDF LUT) ambient terms that match the visible sky and sun — no external HDR map required.

- **Sun position** is derived from `timeOfDay`, `latitude`, and `dayOfYear` via a standard solar-position model (seasonal declination + hour angle). Directional lights flagged as the sun track this automatically.
- **Sky bake** — when `sky.proceduralSky` is enabled, the procedural dome is rendered into a 256²/face RGBA16F cubemap (`ProceduralSkyCapture`) and fed to `IBLSystem::generateFromSkybox`, exactly like a loaded HDR environment. This runs independently of the "Show Skybox" toggle, so a model is lit by the sky even when the sky is not drawn as the background.
- **Re-bake gating** — a full irradiance + prefilter regen is not free, so the bake only re-triggers on a meaningful change, with hysteresis for continuous drivers:
  - `timeOfDay` — accumulated delta `> 0.05h` (a smoothly advancing day/night cycle does not re-bake every frame)
  - `latitude` — delta `> 0.5°`
  - `dayOfYear` — delta `> 1`
  - scattering params (`atmosphereRadius`, `rayleighScaleHeight`, `mieScaleHeight`, `betaRayleigh`, `betaMie`, `mieG`, `skyIntensity`) — always re-bake immediately on any change (rare manual tweaks)
- **Non-procedural fallback** — with `proceduralSky` off, the Yokohama disk skybox + IBL map are loaded as before; with no environment at all, IBL falls back to a black cubemap.

## Ray-Traced Shadows

When the GPU exposes `VK_KHR_ray_query` (Vulkan 1.4 / RADV STRIX_HALO and recent desktop drivers), the deferred lighting pass can resolve direct-light shadows with hardware ray queries instead of rasterized shadow maps.

- **Acceleration structures** — `AccelBuilder` maintains one BLAS per `Model` (built from its vertex/index buffers with `SHADER_DEVICE_ADDRESS_BIT`) and rebuilds a single TLAS every frame from all visible model instances (early-out when the instance set is empty to avoid zero-size buffer creation). The TLAS address is bound into the deferred-lighting descriptor set.
- **Per-light-type toggle** — independent runtime flags (`rtDirectional`, `rtPoint`, `rtSpot`) enable ray tracing per light type. A light type with its flag off falls back transparently to the rasterized cascade/cube/spot shadow map path, so hybrid setups are possible (e.g. ray-traced directional + shadow-mapped point).
- **Soft penumbra** — `rtShadowSoftness` (angular spread in radians) drives a cone-jittered soft shadow: a dominant center ray (hard shadow) blended with two jittered rays, shaped by a contrast curve to keep core shadows dark. Point/spot lights widen the cone to `max(softness, atan(lightRadius, dist))` so area-light softness scales with distance, with a distance LOD that halves softness for far lights.
- **Transparency-aware** — the hard-shadow trace bounces up to 8 times, reading per-submesh opacity from the `InstanceSubmeshHeader`/`submeshEntries` buffers, so transparent geometry partially transmits shadow rays instead of casting a hard black occluder.
- **UI & gating** — shader compilation defines `RAY_TRACING_ENABLED=1` (see `compile_shaders.py`) and the pipeline is created with ray-query support only when `Device::rayQuerySupported()` is true. Toggles and softness are exposed in **Settings → Ray Tracing**.

## Viewport & Picking

- **Navigation mode** -- WASD + mouse-look camera control (right-click or toolbar toggle)
- **Picking mode** -- Click on objects in the viewport to select them
  - Models: ray-triangle intersection against CPU collision geometry (per-triangle, not AABB)
  - Cameras / Lights: screen-space radius test from projected position
  - Gizmo-aware: picking is suppressed while the gizmo is being dragged or hovered
- **Gizmo** -- T/R/S buttons in the toolbar, World/Local toggle
- **View gizmo** -- Always shown in Picking mode; hidden in Navigation mode to reduce interaction noise
  - Orbit around selection is allowed only when the selection has a valid `ModelComponent`
  - Non-model selections use look-in-place reorientation

## Dependencies

| Library          | Purpose                                      |
|------------------|----------------------------------------------|
| GLFW             | Window management, input, Vulkan surface     |
| GLM              | Mathematics (vectors, matrices, quaternions) |
| EnTT             | Entity Component System                      |
| ImGui            | Immediate-mode GUI (docking branch)          |
| ImGuizmo         | 3D transformation gizmo overlay              |
| Jolt Physics     | Multi-core rigid-body physics                |
| TinyGLTF         | glTF 2.0 model/scene loading                 |
| TinyOBJLoader    | OBJ model loading                            |
| TinyEXR          | OpenEXR / HDR texture loading                |
| stb              | LDR texture loading (PNG, JPEG)              |
| nlohmann/json    | JSON serialization                           |
| meshoptimizer    | Vertex cache optimization, meshlets          |
| Google Test      | Unit testing                                 |

## Platform Support

| Platform     | Status |
|--------------|--------|
| Linux (Wayland) | Supported (default) |
| Linux (X11)  | Supported  |
| Windows      | Supported  |

## License

MIT -- see [LICENSE](./LICENSE). 
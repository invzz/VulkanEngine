# Engine2

## Overview

Engine2 is a modern Vulkan-based 3D rendering engine designed for performance and flexibility. It features a data-oriented architecture using Entity Component System (ECS), a robust Render Graph for managing complex rendering pipelines, and support for modern rendering techniques like PBR and IBL.

## Key Features

- **Core Architecture**:
  - **Entity Component System (ECS)**: Powered by `EnTT` for high-performance data-oriented design.
  - **Render Graph**: Flexible dependency graph for managing render passes and resources.
  - **Input System**: Unified input handling.

- **Rendering**:
  - **Vulkan Backend**: Low-level graphics API for maximum control and performance.
  - **Physically Based Rendering (PBR)**: Realistic lighting and materials.
  - **Image Based Lighting (IBL)**: Environment lighting for realistic reflections and ambient light.
  - **Shadows**: Support for Directional, Point, Spot, and Cube Shadow Maps.
  - **Post-Processing**: Integrated post-processing pipeline.
  - **Skybox**: Procedural and Cubemap skybox support.
  - **Dust Rendering**: Volumetric dust effects.
  - **Morph Targets**: Compute shader-based morph target animation.
  - **LOD System**: Level of Detail management for performance optimization.

- **Resources & Assets**:
  - **glTF 2.0 Support**: Full support for loading scenes and models via `tinygltf`.
  - **Mesh Optimization**: Uses `meshoptimizer` for efficient geometry processing.
  - **Texture Management**: Automatic loading and caching of textures.

- **Tools & Debugging**:
  - **ImGui Integration**: Built-in UI for debugging, profiling, and scene inspection.
  - **Scene Serialization**: Save and load scene states.

## Repository Layout

- `include/Engine/`
  - Public headers organized by module (`Core`, `Graphics`, `Resources`, `Scene`, `Systems`).
- `src/Engine/`
  - Implementation files mirroring the include structure.
- `src/demos/`
  - Example applications and demos (e.g., `Cube`).
- `assets/`
  - `shaders/`: GLSL source files.
  - `models/`: 3D models and scenes.
  - `textures/`: Texture assets.
- `xmake.lua`
  - Build configuration.

## Prerequisites

- A Vulkan-capable GPU and drivers.
- `xmake` build system.
- `glslc` (from Vulkan SDK) for shader compilation.
- Development libraries (automatically handled by xmake):
  - GLFW, GLM, Vulkan SDK
  - EnTT, ImGui, TinyGLTF, nlohmann_json, meshoptimizer, stb

## Building

```fish
# Configure and build the debug version
xmake f -m debug
xmake

# Run the Cube demo
xmake run Cube
```

Use `xmake f -m release` for an optimized build.

## Shader Compilation

Shaders are compiled automatically during the build process, but you can manually regenerate them if needed:

```fish
./compile_shaders.sh
```

## Demos

- **Cube**: The main testbed application demonstrating the engine's capabilities, including PBR, shadows, and model loading.


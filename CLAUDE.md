# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a modern Vulkan-based 3D rendering engine designed for performance and flexibility. It features a data-oriented architecture using Entity Component System (ECS), a robust Render Graph for managing complex rendering pipelines, and support for modern rendering techniques like PBR and IBL.

## Key Architecture Components

### EngineState (Single Source of Truth)
- Centralizes runtime state, owned systems, descriptor pools/sets and scene data in a single `EngineState` object
- Simplifies `App` to an orchestrator while render passes and UI panels take an `EngineState*` for access to runtime data
- Improves testability, serialization, and reduces duplicated state across passes and UI panels

### Core Architecture
- **Entity Component System (ECS)**: Powered by `EnTT` for high-performance data-oriented design
- **Render Graph**: Flexible dependency graph for managing render passes and resources
- **Input System**: Unified input handling

### Rendering Features
- **Vulkan Backend**: Low-level graphics API for maximum control and performance
- **Physically Based Rendering (PBR)**: Realistic lighting and materials
- **Image Based Lighting (IBL)**: Environment lighting for realistic reflections and ambient light
- **Shadows**: Support for Directional, Point, Spot, and Cube Shadow Maps
- **Post-Processing**: Integrated post-processing pipeline
- **Skybox**: Procedural and Cubemap skybox support
- **Dust Rendering**: Volumetric dust effects
- **Morph Targets**: Compute shader-based morph target animation
- **LOD System**: Level of Detail management for performance optimization

### Resources & Assets
- **glTF 2.0 Support**: Full support for loading scenes and models via `tinygltf`
- **Mesh Optimization**: Uses `meshoptimizer` for efficient geometry processing
- **Texture Management**: Automatic loading and caching of textures

## Repository Layout

- `include/Engine/`: Public headers organized by module (`Core`, `Graphics`, `Resources`, `Scene`, `Systems`)
- `src/Engine/`: Implementation files mirroring the include structure
- `src/demos/`: Example applications and demos (e.g., `Cube`)
- `assets/`: 
  - `shaders/`: GLSL source files
  - `models/`: 3D models and scenes
  - `textures/`: Texture assets
- `xmake.lua`: Build configuration

## Development Setup

### Prerequisites
- A Vulkan-capable GPU and drivers
- `xmake` build system
- `glslc` (from Vulkan SDK) for shader compilation
- Development libraries (automatically handled by xmake):
  - GLFW, GLM, Vulkan SDK
  - EnTT, ImGui, TinyGLTF, nlohmann_json, meshoptimizer, stb

### Building
```fish
# Configure and build the debug version
xmake f -m debug
xmake

# Run the Cube demo
xmake run Cube
```

Use `xmake f -m release` for an optimized build.

Validation defaults:
- Debug mode: validation enabled
- Release mode: validation disabled

Force validation in any mode:
```fish
xmake f -m release --validation=y
xmake
```

Runtime override flags:
- `--validation` or `--validation=on` enables validation
- `--no-validation` or `--validation=off` disables validation

Example:
```fish
xmake run Editor -- --validation=off
```

### Shader Compilation
Shaders are compiled automatically during the build process, but you can manually regenerate them if needed:
```fish
./compile_shaders.sh
```

## Common Development Tasks

- Build: `xmake`
- Run debug build: `xmake run Editor`
- Run release build: `xmake f -m release && xmake run Editor`
- Run tests: `xmake run Tests` (or `xmake run Tests -- --gtest_filter=TestSuiteName.*`)
- Format code: `./format_code.sh`
- Clean build: `xmake f -c`

## Key Files and Directories

### Core Engine Components
- `include/Engine/EngineState.hpp`: Main engine state container with systems, resources, and scene data
- `src/Editor/main.cpp`: Entry point for the main editor application
- `include/Engine/Systems/ModelRenderSystem.hpp`: Main system for rendering 3D models with PBR support

### Rendering Systems
- `include/Engine/Graphics/FrameGraph/`: Render graph implementation for managing render passes
- `include/Engine/Systems/`: Various rendering systems (LightSystem, ShadowSystem, etc.)
- `assets/shaders/`: GLSL shader source files organized by functionality

### Scene Management
- `include/Engine/Scene/`: Scene hierarchy and entity management with EnTT ECS
- `include/Engine/Systems/AnimationSystem.hpp`: Animation handling system
- `include/Engine/Systems/CameraSystem.hpp`: Camera management system

## Testing
The engine includes a test suite using Google Test. Tests can be run via:
```fish
xmake run Tests
```
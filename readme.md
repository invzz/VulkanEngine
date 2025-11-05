# Engine2 Demo

## Overview

Engine2 is a Vulkan-based playground that drives several small demos. The current focus is the `ThreeBodiesSimulation`, a 2D n-body experiment with gravity, collision/merge logic, and particle trails. The project uses **xmake** for builds and relies on GLFW and GLM for windowing and math support.

## Repository Layout

- `include/2dEngine/`
  - Public headers for the reusable engine components.
- `src/2dEngine/`
  - Engine implementation files (`Device`, `Renderer`, `Pipeline`, etc.).
- `src/demos/ThreeBodiesSimulation/`
  - Entry point and gameplay systems for the gravity demo.
- `assets/shaders/`
  - GLSL sources; compiled SPIR-V outputs land in `assets/shaders/compiled/`.
- `build/`
  - Generated binaries arranged by platform/configuration (ignored by git).
- `compile_shaders.sh`
  - Helper script that runs `glslc` to regenerate SPIR-V outputs.
- `xmake.lua`
  - Build definition describing targets and dependencies.

## Prerequisites

- A Vulkan-capable GPU and drivers.
- `xmake`
- `glslc` (from the Vulkan SDK) to rebuild shaders.
- GLFW, GLM, and volk development headers/libraries (xmake fetches them automatically when possible).

## Building

```fish
# Configure and build the debug version
xmake f -m debug
xmake

# Run the ThreeBodiesSimulation demo
xmake run ThreeBodiesSimulation
```

Use `xmake f -m release` for an optimized build. Output binaries land under `build/linux/x86_64/<mode>/` by default.

## Shader Compilation

If you modify GLSL files in `assets/shaders/`, regenerate the SPIR-V binaries:

```fish
./compile_shaders.sh
```

The script writes updated `.spv` files into `assets/shaders/compiled/`. Ensure `glslc` is on your PATH.

## Configuration

Simulation constants live in `src/demos/ThreeBodiesSimulation/SimulationConfig.hpp`. Values control gravity strength, initial body layout, collision behaviour, and trail appearance. Adjust them to tune the demo.

## Demos

- `ThreeBodiesSimulation`: interactive 2D n-body sandbox with gravity, collisions, and particle trails.

## Next Steps

- Wire up CI to build the engine library and demo across debug/release configurations.
- Publish additional demos or tooling notes in the README as they ship.
- Consider generating `compile_commands.json` for improved IDE integration.

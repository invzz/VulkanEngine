# Shader Compilation and Management in Vulkan Engine

## Overview

The Vulkan engine manages shaders through a comprehensive system that includes shader source files, compilation scripts, and integration into the rendering pipeline. The system supports both traditional vertex/fragment shaders and modern mesh/task shaders for efficient GPU processing.

## Shader Source Files

Shader source files are located in `assets/shaders/` directory and include:
- Vertex shaders (.vert)
- Fragment shaders (.frag) 
- Compute shaders (.comp)
- Task shaders (.task) - for mesh shader task stages
- Mesh shaders (.mesh) - for mesh shader mesh stages

Example shader files:
- `cube_shadow.vert`, `cube_shadow.frag`
- `shadow.task`, `simple_mesh.task`
- `pbr_shader.frag` (main PBR shader with variants)
- `hiz_generate.comp` (hierarchical Z-buffer generation)

## Shader Compilation Process

### 1. Compilation Script
The engine uses `compile_shaders.sh` script for compiling GLSL shaders to SPIR-V binary format:
- Uses `glslc` compiler with target SPIR-V version 1.5
- Supports include paths via `-I` flags
- Compiles all shader types (.vert, .frag, .comp, .task, .mesh)
- For PBR fragment shaders, it builds both standard and full variants

### 2. Shader Variants
The compilation script supports shader variant management:
- The `pbr_shader.frag` shader has a "standard" variant that disables expensive features
- This is controlled by the `ENGINE_BUILD_STANDARD_VARIANT` environment variable (default: 1)
- Standard variant compilation uses preprocessor defines to disable features like:
  - PBR_ENABLE_DEBUG=0
  - PBR_ENABLE_SPEC_GLOSS=0
  - PBR_ENABLE_IRIDESCENCE=0
  - etc.

### 3. Output Location
Compiled shaders are stored in `assets/shaders/compiled/` directory with `.spv` extension:
- `pbr_shader.frag.spv`
- `pbr_shader_standard.frag.spv` (standard variant)
- `simple_mesh.task.spv`
- `simple_mesh.mesh.spv`
- etc.

## Shader Integration into Engine

### 1. Pipeline Management
Shaders are loaded and integrated through the Pipeline class in `src/Engine/Graphics/Pipeline.cpp`:
- Supports both traditional graphics pipelines (vertex + fragment) and mesh pipelines (task + mesh + fragment)
- Loads SPIR-V binary files using `readFile()` method
- Creates Vulkan shader modules with `createShaderModule()`
- Builds graphics pipelines with appropriate shader stages

### 2. Path Configuration
The `xmake.lua` build file defines the `SHADER_PATH` preprocessor macro:
```lua
add_defines(
    "SHADER_PATH=\""  .. normpath(path.join(project_dir, "assets/shaders/compiled")) .. "/\""
)
```
This path is used throughout the engine to reference compiled shader files.

### 3. Mesh Shader Support
The engine implements mesh shaders (task + mesh + fragment pipeline):
- Task shaders for culling and task generation 
- Mesh shaders for vertex processing and primitive assembly
- Fragment shaders for final pixel shading
- Uses `VK_SHADER_STAGE_TASK_BIT_EXT`, `VK_SHADER_STAGE_MESH_BIT_EXT`, and `VK_SHADER_STAGE_FRAGMENT_BIT`

### 4. Hot Reloading
The engine supports shader hot-reloading:
- `Pipeline::reloadIfChanged()` checks modification times of shader files
- `ModelRenderSystem::hotReloadPipelinesIfNeeded()` manages pipeline reloading
- Allows real-time shader updates during development

## Key Features

1. **Modern Shader Pipeline**: Supports mesh shaders for efficient GPU processing
2. **Variant Management**: PBR shader supports optimized standard variants
3. **Hot Reloading**: Real-time shader reloading during development
4. **Cross-Platform**: Works on Linux and Windows with proper Vulkan SDK setup
5. **Include Support**: Shader includes via the `includes/` directory structure

## Usage Examples

In `ModelRenderSystem.cpp`, pipelines are created like:
```cpp
transparentPipeline = std::make_unique<Pipeline>(device,
    std::string(SHADER_PATH) + R"(simple_mesh.task.spv)",
    std::string(SHADER_PATH) + R"(simple_mesh.mesh.spv)",
    std::string(SHADER_PATH) + R"(pbr_shader.frag.spv)",
    transparentConfig);
```

The system demonstrates a robust approach to shader management that supports modern Vulkan features while providing flexibility for different rendering requirements.
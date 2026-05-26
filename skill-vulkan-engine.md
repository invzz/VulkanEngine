# Vulkan Engine Developer Assistant

## Description
This skill helps developers understand and work efficiently with the Vulkan-based 3D rendering engine. It provides guidance on the engine's architecture, key components, development workflows, and best practices for working with its data-oriented design using ECS, Render Graph, PBR/IBL, and modern graphics techniques.

## When to Use This Skill
Use this skill when:
- Learning about the engine's architecture and core components
- Understanding how to work with EngineState and its systems (ModelRenderSystem, ShadowSystem, LightSystem, etc.)
- Getting guidance on development workflows and patterns for Vulkan rendering
- Needing help with specific rendering techniques like PBR, IBL, or shadows
- Working with ECS patterns in EnTT
- Understanding the render graph and pipeline management
- Debugging rendering issues or performance bottlenecks

## Key Architecture Components

### EngineState (Single Source of Truth)
The `EngineState` class is the central hub that manages:
- All systems (ModelRenderSystem, ShadowSystem, LightSystem, etc.)
- Resources (descriptor pools, sets, scene data)
- Scene hierarchy and entities
- Input devices and UI managers
- Descriptor/layout state used by several passes

### Core Architecture
- **Entity Component System (ECS)**: Powered by `EnTT` for high-performance data-oriented design
- **Render Graph**: Flexible dependency graph for managing render passes and resources
- **Input System**: Unified input handling

### Rendering Systems Overview
1. **ModelRenderSystem** - Main system for rendering 3D models with PBR support using mesh shaders and GPU culling
2. **LightSystem** - Handles point, directional, and spot light rendering with different pipeline configurations
3. **ShadowSystem** - Manages shadow map rendering for directional, spot, and point lights using mesh shaders with CPU frustum culling
4. **IBLSystem** - Implements Image Based Lighting for realistic reflections and ambient lighting
5. **PostProcessingSystem** - Integrated post-processing pipeline for effects like bloom, tonemapping, etc.

### Rendering Features
- **Vulkan Backend**: Low-level graphics API for maximum control and performance
- **Physically Based Rendering (PBR)**: Realistic lighting and materials with metallic/roughness workflows
- **Image Based Lighting (IBL)**: Environment lighting for realistic reflections and ambient light
- **Shadows**: Support for Directional, Point, Spot, and Cube Shadow Maps using mesh shaders with GPU culling
- **Post-Processing**: Integrated post-processing pipeline with effects like bloom, tonemapping, etc.
- **Skybox**: Procedural and Cubemap skybox support
- **Dust Rendering**: Volumetric dust effects

## Development Workflows

### Building and Running
```fish
# Configure and build the debug version
xmake f -m debug
xmake

# Run the Cube demo
xmake run Cube

# Run with validation enabled
xmake run Editor -- --validation=on
```

### Key Files to Understand
- `include/Engine/EngineState.hpp`: Main engine state container with all systems and resources
- `src/Editor/main.cpp`: Entry point for the main editor application  
- `include/Engine/Systems/ModelRenderSystem.hpp`: Main system for rendering 3D models with PBR support
- `include/Engine/Systems/LightSystem.hpp`: Light rendering system
- `include/Engine/Systems/ShadowSystem.hpp`: Shadow map rendering system

### Common Development Tasks

#### Adding New Rendering Systems
1. Create a new system header in `include/Engine/Systems/`
2. Add the system as a unique_ptr member to EngineState class
3. Register the system in the appropriate registration function (registerCoreSystems, registerPostProcessing, etc.)
4. Initialize the system in initCoreSystems or corresponding initialization function

#### Working with Resources
- Use ResourceManager for asset loading and caching of models, textures, etc.
- Manage descriptor pools and sets properly for shader resources
- Work with EngineState's resourceState() to access shared resources

#### Scene Management
- Access scene entities through EngineState's sceneState()
- Use the ECS pattern for flexible entity behavior
- Work with camera entities and selection systems

#### Descriptor Management
- The engine uses descriptor pools and sets for efficient GPU resource binding
- Systems like ModelRenderSystem, LightSystem, and ShadowSystem manage their own descriptor resources
- Descriptor layouts are shared across multiple systems when appropriate

## Best Practices

### ECS Usage
- Leverage EnTT's data-oriented design for efficient processing of entities
- Use component-based architecture for flexible entity behavior
- Minimize system coupling by accessing only required data through EngineState

### Rendering Pipeline
- Utilize the Render Graph for managing complex rendering dependencies
- Follow proper resource lifecycle management with descriptor pools
- Implement PBR materials correctly with appropriate metallic/roughness textures
- Use mesh shaders for efficient GPU culling in shadow rendering

### Performance Considerations
- Use Level of Detail (LOD) system for performance optimization
- Implement morph target animations efficiently using compute shaders
- Leverage shadow mapping techniques appropriately for scene complexity
- Minimize state changes and batch draw calls where possible
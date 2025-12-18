# Fog & Volumetric Lighting Implementation Plan

## Phase 1: Basic Fog (Tier 1 - "Must Have")
- [x] **Data Structures**:
    - Update `GlobalUbo` in `FrameInfo.hpp` to include fog parameters (color, density, height, falloff).
    - Create `FogSettings` struct in `SkyboxRenderSystem.hpp`.
- [x] **Shader Implementation**:
    - Update `assets/shaders/pbr_shader.frag` to receive fog parameters.
    - Implement Exponential Fog: `1.0 - exp(-distance * density)`.
    - Implement Height Fog: `exp(-(worldPos.y - height) * heightDensity)`.
    - Implement Sky-Color Matching: Mix horizon and zenith colors based on view direction.
- [x] **Engine Integration**:
    - Update `App` class to manage `FogSettings`.
    - Update `App::shadowPhase` (where UBO is updated) to calculate dynamic sky colors (Day/Sunset/Night) and pass them to the shader.
- [x] **UI**:
    - Update `SettingsPanel` to expose fog controls (Density, Height, Color, Use Sky Color).

## Phase 2: Volumetric Lighting (Tier 2 - "God Rays")
- [x] **Strategy Selection**:
    - Target: Screen-Space Light Scattering (Radial Blur).
    - Constraint: Nvidia T600 (Mid-range).
- [x] **Implementation**:
    - [x] **Shader**: Updated `assets/shaders/post_process.frag` to include radial blur logic using depth buffer for occlusion.
    - [x] **System**: Updated `PostProcessingSystem` and `PostProcessPushConstants` to support God Ray parameters.
    - [x] **App Logic**: Updated `App.cpp` to calculate Sun Screen Position and pass it to the shader.
    - [x] **UI**: Updated `SettingsPanel` to expose God Ray controls.

## Phase 3: Refinement (Future)
- [ ] **Point/Spot Volumetrics**: Requires volumetric buffer.
- [ ] **Temporal Reprojection**: To improve quality/performance.

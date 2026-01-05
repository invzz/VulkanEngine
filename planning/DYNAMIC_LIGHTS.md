# Dynamic lights: removing fixed limits

This engine currently hard-limits the number of dynamic lights because light data is stored in a single scene UBO with fixed-size arrays.

- Shader side: [assets/shaders/includes/scene_ubo.glsl](../assets/shaders/includes/scene_ubo.glsl) has `pointLights[16]`, `directionalLights[16]`, `spotLights[16]`.
- CPU side: [src/Engine/Systems/LightSystem.cpp](../src/Engine/Systems/LightSystem.cpp) writes into those arrays and asserts against a fixed `maxLightCount`.
- Shadows are also hard-limited by descriptor arrays (currently `shadowMaps[4]` and `cubeShadowMaps[4]`) in deferred lighting.

The right approach for “any number of dynamic lights” in Vulkan is:
1) move light lists out of UBOs into GPU buffers (SSBOs), and
2) avoid looping over all lights per pixel by doing tiled/clustered light culling.

This document outlines what to change.

---

## Goal and definition

- “Any number” means: the number of lights is no longer a compile-time fixed array size; it becomes “as many as fit in GPU memory / budgets”, with runtime scaling.
- Practically: you still need *performance* controls (culling, max lights per tile/cluster, shadow budgets).

---

## 1) Split the scene UBO into “small UBO + SSBO light buffers”

### Current problem
UBOs have size limits and are a poor fit for variable-length arrays. Today the shader interface hardcodes:
- `PointLight pointLights[16]`
- `DirectionalLight directionalLights[16]`
- `SpotLight spotLights[16]`

### Target design
Keep only camera / global constants in a small UBO, and move lights into storage buffers:

- **Global UBO** (small, fixed): camera matrices, camera position, ambient, fog, debug flags, etc.
- **Light SSBO(s)** (variable length): arrays of lights.

Two common layouts:

**Option A (separate arrays per type)**
- `PointLight[]`, `SpotLight[]`, `DirectionalLight[]` in separate SSBOs.
- Separate counts in a small UBO or a tiny SSBO header.

**Option B (unified light array)**
- One `GPULight[]` where each element has a `type` field and a union-like payload.
- One count.
- Works very well with clustered/tiled lists because the list is just indices into one array.

Recommendation: **Option B** (simplifies culling, shadow indexing, and future light types).

### Vulkan binding changes
- Add a descriptor binding for the light SSBO (and later the tile/cluster index buffers).
- Use a ring-buffer or per-frame buffer allocation strategy (frames-in-flight) so updates don’t stall.

---

## 2) Update CPU-side light collection and upload

### Current state
[src/Engine/Systems/LightSystem.cpp](../src/Engine/Systems/LightSystem.cpp) writes directly into a fixed `GlobalUbo` struct.

### What to do
- Replace the UBO-array writes with pushes into a CPU-side vector, e.g. `std::vector<GPULight>`.
- Upload that vector every frame to a mapped staging buffer and copy to a device-local SSBO (or use persistently mapped host-visible SSBO if acceptable).

Key points:
- Keep a stable mapping of “light index” for the frame (the shader references lights by index).
- Store counts alongside the buffer (either in a small UBO, or as the first element of a header SSBO).

---

## 3) Add tiled/clustered light culling (required for scale)

### Why
If you support 100s–1000s of lights, a fullscreen deferred fragment shader looping over all lights is not viable.

### Minimal scalable solution: Tiled deferred (screen tiles)
- Divide the screen into tiles (commonly 16×16 pixels).
- Run a compute shader that:
  - computes a per-tile frustum (or uses depth min/max per tile),
  - tests each light’s bounds (sphere for point, cone/sphere for spot) against the tile frustum,
  - writes a list of light indices for that tile into a global index buffer.

Outputs:
- `tileLightCount[tileId]`
- `tileLightOffset[tileId]`
- `tileLightIndices[]` (packed)

Then in deferred lighting:
- compute tile id from `gl_FragCoord`,
- iterate only the light indices for that tile.

### Better scaling: Clustered deferred (3D clusters)
- Split screen into tiles (x,y) and depth slices (z).
- Great when many lights overlap in screen but occupy different depth ranges.

Recommendation: start with **tiled** (simpler), then upgrade to **clustered** if needed.

---

## 4) Shadows: decouple “shadowed lights” from “all lights”

### Current state
Deferred lighting binds fixed descriptor arrays for shadows (small N).

### Reality check
Even if you allow *unlimited* lights, you usually cannot afford *unlimited shadow maps*.

So treat shadows as a budgeted resource:

- Maintain two concepts:
  - **All lights**: unlimited (SSBO).
  - **Shadowed lights**: limited by an explicit budget (e.g. 4–32) based on importance.

### Shadow indexing strategy
- Each `GPULight` can carry a `shadowIndex` (or `-1` / `UINT_MAX` for no shadow).
- Shadow resources are stored separately:
  - 2D shadows for directional/spot
  - cube shadows for point

### Vulkan resource strategy options

**Option A: Array textures / atlases (recommended where possible)**
- 2D shadows: `sampler2DArrayShadow` for depth compares.
- Point shadows: cube-map array images if supported (`samplerCubeArray`), or pack faces into a 2D array/atlas and do manual compare.

**Option B: Bindless descriptor indexing (VK_EXT_descriptor_indexing)**
- Store many shadow maps in descriptor arrays and index them dynamically.
- More flexible, but needs descriptor indexing enabled and careful layout.

Recommendation: keep shadows budgeted, and prefer array textures/atlases first.

---

## 5) Shader changes required

### Replace fixed arrays
- Update [assets/shaders/includes/scene_ubo.glsl](../assets/shaders/includes/scene_ubo.glsl):
  - Remove `pointLights[16]`, `directionalLights[16]`, `spotLights[16]`.
  - Keep only global constants.

### Add SSBO definitions
- Add something like:
  - `layout(set=0,binding=X) readonly buffer Lights { GPULight lights[]; };
  - `layout(set=0,binding=Y) readonly buffer LightHeader { uint lightCount; ... };

### Add tile/cluster list access
- Add SSBOs for `tileLightOffset`, `tileLightCount`, and `tileLightIndices`.
- In deferred lighting, loop only the tile’s index list.

---

## 6) Engine / pipeline wiring

- Descriptor set layouts:
  - Add bindings for light SSBO(s) and tile/cluster SSBO(s).
  - Add bindings for shadow atlas/arrays (or bindless descriptor arrays).

- Render graph / frame order:
  - After depth is available (after G-buffer depth exists), run the light culling compute pass.
  - Then run deferred lighting fullscreen pass that consumes the per-tile/cluster lists.

---

## 7) Debugging + validation (worth doing early)

Add debug visualizations so you can prove the system works:
- Tile heatmap: display `tileLightCount` as a grayscale overlay.
- “Show light volume overlaps” toggle.
- Per-pixel “lights evaluated” counter (optional).

Also validate correctness:
- Compare old fixed-array lighting vs new SSBO lighting with identical small light counts.
- Stress test with 100/500/2000 lights; confirm performance scales with overlap, not total count.

---

## Practical milestones

1) Move light arrays out of UBO into an SSBO; keep looping over all lights (still capped by performance, but removes the hard array limit).
2) Add tiled light culling compute and per-tile index lists.
3) Integrate shadow budgets and shadow indexing.
4) Optional: upgrade tiled → clustered if needed.

---

## Notes specific to the current codebase

- UBO fixed limits are visible in [assets/shaders/includes/scene_ubo.glsl](../assets/shaders/includes/scene_ubo.glsl).
- CPU asserts enforcing caps are visible in [src/Engine/Systems/LightSystem.cpp](../src/Engine/Systems/LightSystem.cpp).
- Shadow map caps are enforced in [src/Engine/Systems/ShadowSystem.cpp](../src/Engine/Systems/ShadowSystem.cpp) and by fixed shader descriptor arrays.

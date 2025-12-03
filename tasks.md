# Engine Development Tasks

## Overview
Prioritized task list for engine improvements. Focus: **Visual Quality First**, then workflow, then architecture.

**Start Date:** December 2025  
**Approach:** Feature-first, iterate quickly, measure before optimizing

---

## 📍 Current Sprint: Visual Quality

### Task 1: Skybox System
**Priority:** 🔥 High | **Estimate:** 1-2 days | **Status:** ✅ Complete

**Goal:** Replace black void with environment cubemap

**Subtasks:**
- [x] Create `Skybox` class with cubemap texture loading
- [x] Create skybox vertex/fragment shaders (simple cube projection)
- [x] Create `SkyboxRenderSystem` 
- [x] Render skybox first (no depth test, everything overdraws)
- [x] Add skybox to demo scene
- [x] Fix Y-axis flip for correct orientation
- [ ] Support HDR cubemaps (.hdr format) for future IBL

**Files created:**
```
include/3dEngine/Skybox.hpp
src/3dEngine/Skybox.cpp
include/3dEngine/systems/SkyboxRenderSystem.hpp
src/3dEngine/systems/SkyboxRenderSystem.cpp
assets/shaders/skybox.vert
assets/shaders/skybox.frag
```

**Result:** Yokohama cubemap skybox renders correctly as environment background

---

### Task 2: Shadow System (Directional Light)
**Priority:** 🔥 High | **Estimate:** 3-4 days | **Status:** ✅ Complete

**Goal:** Directional light casts shadows on all objects

**Subtasks:**
- [x] Create `ShadowMap` class (depth-only framebuffer)
- [x] Create shadow map render pass (depth only, no color)
- [x] Create `ShadowSystem` to manage shadow rendering
- [x] Create shadow vertex shader (simple transform to light space)
- [x] Calculate light-space matrix for directional light
- [x] Add shadow map sampler to PBR shader (set 2)
- [x] Implement PCF filtering (3x3) for soft edges
- [x] Add shadow bias to prevent acne
- [x] Integrate into main render loop (shadow pass before main pass)

**Files created:**
```
include/3dEngine/ShadowMap.hpp
src/3dEngine/ShadowMap.cpp
include/3dEngine/systems/ShadowSystem.hpp
src/3dEngine/systems/ShadowSystem.cpp
assets/shaders/shadow.vert
assets/shaders/shadow.frag
```

**Files modified:**
```
assets/shaders/pbr_shader.frag       (added shadow map sampling, PCF)
include/3dEngine/FrameInfo.hpp       (added lightSpaceMatrices, shadowLightCount)
include/3dEngine/systems/PBRRenderSystem.hpp  (added shadow descriptor set)
src/3dEngine/systems/PBRRenderSystem.cpp      (shadow descriptor binding)
src/demos/Cube/app.hpp               (added ShadowSystem to GameLoopState)
src/demos/Cube/app.cpp               (added shadowPhase)
```

**Result:** Directional light shadows with PCF soft edges and depth bias

---

### Task 3: Bloom Post-Process
**Priority:** 🔥 High | **Estimate:** 2 days | **Status:** ⬜ Not Started

**Goal:** Bright areas glow, lights look emissive

**Subtasks:**
- [ ] Create `PostProcessStack` class
- [ ] Create offscreen framebuffer for HDR rendering
- [ ] Create brightness extraction shader (threshold filter)
- [ ] Create gaussian blur shaders (horizontal + vertical passes)
- [ ] Create bloom composite shader (additive blend)
- [ ] Create downscale chain (4-5 levels for wide bloom)
- [ ] Integrate into render loop (after main pass, before UI)
- [ ] Add bloom intensity/threshold controls to UI

**Files to create:**
```
include/3dEngine/PostProcess.hpp
src/3dEngine/PostProcess.cpp
assets/shaders/fullscreen_quad.vert
assets/shaders/brightness_extract.frag
assets/shaders/gaussian_blur.frag
assets/shaders/bloom_composite.frag
```

**Test:** Point lights have soft glow halos, bright surfaces bloom

---

## 📍 Next Sprint: Workflow & Debug

### Task 4: Profiler System
**Priority:** 🟡 Medium | **Estimate:** 2 days | **Status:** ⬜ Not Started

**Goal:** See CPU and GPU timing for each phase

**Subtasks:**
- [ ] Create `Profiler` class with scoped timing
- [ ] Add Vulkan timestamp queries for GPU timing
- [ ] Create ImGui panel for profiler display
- [ ] Add hierarchical timing (nested scopes)
- [ ] Add frame time graph (history)
- [ ] Instrument main phases (update, shadow, render, UI)

**Files to create:**
```
include/3dEngine/Profiler.hpp
src/3dEngine/Profiler.cpp
src/demos/Cube/ui/ProfilerPanel.hpp
```

**Test:** See "PBR Render: 2.3ms GPU" in profiler panel

---

### Task 5: Hot Shader Reload
**Priority:** 🟡 Medium | **Estimate:** 1-2 days | **Status:** ⬜ Not Started

**Goal:** Edit shader, see changes without restart

**Subtasks:**
- [ ] Create `FileWatcher` class (poll file timestamps)
- [ ] Detect shader source file changes
- [ ] Recompile GLSL → SPIR-V on change (call glslc)
- [ ] Recreate pipeline with new shader
- [ ] Handle compilation errors gracefully (keep old pipeline)
- [ ] Add UI indicator when shaders are reloaded

**Files to create:**
```
include/3dEngine/FileWatcher.hpp
src/3dEngine/FileWatcher.cpp
```

**Test:** Edit pbr_shader.frag, save, see changes in 1-2 seconds

---

### Task 6: Debug Visualization
**Priority:** 🟡 Medium | **Estimate:** 1-2 days | **Status:** ⬜ Not Started

**Goal:** Draw debug lines, boxes, spheres for debugging

**Subtasks:**
- [ ] Create `DebugRenderer` class (immediate mode API)
- [ ] Implement line rendering (simple vertex buffer)
- [ ] Add drawLine(), drawBox(), drawSphere() methods
- [ ] Add drawFrustum() for camera/light debugging
- [ ] Create debug line shader (simple color, no lighting)
- [ ] Toggle visibility via ImGui checkbox
- [ ] Draw bounding boxes for selected objects

**Files to create:**
```
include/3dEngine/DebugRenderer.hpp
src/3dEngine/DebugRenderer.cpp
assets/shaders/debug_line.vert
assets/shaders/debug_line.frag
```

**Test:** See wireframe box around selected object, light frustums visible

---

## 📍 Future Sprint: Advanced Lighting

### Task 7: Image-Based Lighting (IBL)
**Priority:** 🟢 Normal | **Estimate:** 3-4 days | **Status:** ⬜ Not Started

**Goal:** Environment provides ambient lighting and reflections

**Subtasks:**
- [ ] Generate diffuse irradiance map from skybox (prefilter)
- [ ] Generate specular prefiltered map (mipmap chain)
- [ ] Generate BRDF LUT texture (2D lookup table)
- [ ] Add IBL textures to PBR shader
- [ ] Blend IBL with direct lighting
- [ ] Support HDR environment maps

**Depends on:** Task 1 (Skybox)

---

### Task 8: Cascaded Shadow Maps (CSM)
**Priority:** 🟢 Normal | **Estimate:** 3-4 days | **Status:** ⬜ Not Started

**Goal:** High-quality shadows at all distances

**Subtasks:**
- [ ] Split view frustum into 3-4 cascades
- [ ] Render shadow map for each cascade
- [ ] Select cascade in fragment shader based on depth
- [ ] Blend between cascades for smooth transitions
- [ ] Visualize cascade splits (debug mode)

**Depends on:** Task 2 (Shadow System)

---

### Task 9: Point Light Shadows
**Priority:** 🟢 Normal | **Estimate:** 3-4 days | **Status:** ⬜ Not Started

**Goal:** Point lights cast omnidirectional shadows

**Subtasks:**
- [ ] Create cubemap shadow atlas
- [ ] Render 6 faces per point light
- [ ] Sample cubemap shadow in PBR shader
- [ ] Optimize: only shadow nearby point lights

**Depends on:** Task 2 (Shadow System)

---

## 📍 Future Sprint: Architecture

### Task 10: Scene Serialization
**Priority:** 🟢 Normal | **Estimate:** 2-3 days | **Status:** ⬜ Not Started

**Goal:** Save and load scenes as JSON files

**Subtasks:**
- [ ] Define JSON scene format
- [ ] Create `SceneSerializer` class
- [ ] Serialize GameObjects (transform, components)
- [ ] Serialize lights and materials
- [ ] Add Save/Load buttons to UI
- [ ] Handle missing assets gracefully

---

### Task 11: SSAO
**Priority:** 🟢 Normal | **Estimate:** 2-3 days | **Status:** ⬜ Not Started

**Goal:** Contact shadows in corners and crevices

**Subtasks:**
- [ ] Create SSAO shader (hemisphere sampling)
- [ ] Generate random kernel and noise texture
- [ ] Add blur pass (bilateral filter)
- [ ] Integrate with lighting (multiply ambient)
- [ ] Add radius/intensity controls

**Depends on:** Task 3 (Post-Process Stack)

---

### Task 12: Deferred Rendering
Let me add some debug output to understand what's happening. Let me first check if the shadow is being rendered at all by visualizing the light space coordinates:

Read pbr_shader.frag, lines 115 to 165

Let me add a strong debug visualization to see if we're even entering the shadow calculation:

Read pbr_shader.frag, lines 475 to 495

pbr_shader.frag+18-0

bash compile_shaders.sh 2>&1 | grep -E "(error|pbr)" && xmake run Cube
Checked background terminal output

The app is running. What colors do you see on the scene?


**Priority:** 🔵 Low | **Estimate:** 5-7 days | **Status:** ⬜ Not Started

**Goal:** Support 100+ lights efficiently

**Subtasks:**
- [ ] Create G-Buffer (albedo, normal, metallic/roughness, depth)
- [ ] Create geometry pass (fill G-Buffer)
- [ ] Create lighting pass (full-screen quad per light)
- [ ] Handle transparency (forward pass for transparent)
- [ ] Migrate existing effects to deferred

**When:** Only if you need many lights (>16)

---

## 📊 Progress Tracker

| Task | Priority | Estimate | Status | Completed |
|------|----------|----------|--------|-----------|
| 1. Skybox | 🔥 High | 1-2 days | ✅ | Done |
| 2. Shadows | 🔥 High | 3-4 days | ✅ | Done |
| 3. Bloom | 🔥 High | 2 days | ⬜ | - |
| 4. Profiler | 🟡 Medium | 2 days | ⬜ | - |
| 5. Hot Reload | 🟡 Medium | 1-2 days | ⬜ | - |
| 6. Debug Viz | 🟡 Medium | 1-2 days | ⬜ | - |
| 7. IBL | 🟢 Normal | 3-4 days | ⬜ | - |
| 8. CSM | 🟢 Normal | 3-4 days | ⬜ | - |
| 9. Point Shadows | 🟢 Normal | 3-4 days | ⬜ | - |
| 10. Scene Save | 🟢 Normal | 2-3 days | ⬜ | - |
| 11. SSAO | 🟢 Normal | 2-3 days | ⬜ | - |
| 12. Deferred | 🔵 Low | 5-7 days | ⬜ | - |

**Legend:**
- ⬜ Not Started
- 🔄 In Progress  
- ✅ Complete
- ❌ Blocked

---

## 🎯 Milestones

### Milestone 1: "It Looks Real" (Week 2)
- [x] PBR Rendering ✅
- [x] Textures ✅
- [x] Skybox ✅
- [x] Shadows ✅
- [ ] Bloom

### Milestone 2: "Developer Friendly" (Week 4)
- [ ] Profiler
- [ ] Hot Reload
- [ ] Debug Visualization
- [ ] Scene Save/Load

### Milestone 3: "Production Quality" (Week 6+)
- [ ] IBL
- [ ] SSAO
- [ ] Cascaded Shadows
- [ ] Point Light Shadows

---

## Notes

- **Always keep it running** - Don't break the build for more than a few hours
- **Test each feature** - Small incremental changes, verify visually
- **Commit often** - One commit per subtask is ideal
- **Skip if blocked** - Move to next task if stuck, come back later

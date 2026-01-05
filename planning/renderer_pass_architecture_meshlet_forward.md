# Renderer Pass Architecture (Desktop)

**Target**: Desktop GPUs (DX12/Vulkan class)

**Features**:
- Task + Mesh shaders (meshlets)
- GPU-driven culling (frustum, screen-size, HZB)
- Real shader variants (compile-time)
- Forward+ clustered lighting (any number of dynamic lights)
- Shadows, transparency, debug tooling

---

## Frame Overview

```text
CPU Frame Setup
    ↓
Depth Prepass (Opaque)
    ↓
HZB Build
    ↓
Light Clustering (Compute)
    ↓
Opaque Main Pass
    ↓
Sky / Atmosphere
    ↓
Transparent Pass
    ↓
Post Processing
    ↓
Debug Overlays
    ↓
UI
```

Shadows execute before the main pass (or in parallel via async queues).

---

## 0. CPU Frame Setup

**Purpose**: Prepare all per-frame data for GPU execution.

- Update camera matrices
- Update global UBO
- Upload light SSBO (dynamic count)
- Reset per-frame GPU buffers
- Build draw ranges / meshlet ranges
- Select shader variants (standard vs full)

---

## 1. Depth Prepass (Opaque)

**Purpose**:
- Populate depth buffer
- Enable early-Z
- Feed HZB and light clustering

**Shaders**:
- Task: frustum-only
- Mesh: position-only
- Fragment: depth-only

**State**:
- Depth test: ON
- Depth write: ON
- Color write: OFF

**Notes**:
- Alpha-masked geometry may need a separate prepass

---

## 2. Hi-Z / HZB Build

**Purpose**:
- Build mipmapped depth pyramid for occlusion culling

**Implementation**:
- Compute shader or fullscreen downsample
- Output: `hzbTexture`

---

## 3. Light Clustering (Forward+)

**Purpose**:
- Support any number of dynamic lights
- Reduce per-fragment light loops

**Implementation**:
- Compute shader
- Screen divided into 3D clusters (x, y, z)

**Inputs**:
- Depth buffer or min/max depth per tile
- Light SSBO

**Outputs**:
- `clusterLightIndices[]`
- `clusterOffsets[]`
- `clusterCounts[]`

---

## 4. Opaque Main Pass

**Purpose**:
- Main shaded geometry

**Shaders**:
- Task: full (frustum + screen-size + HZB)
- Mesh: full meshlet decode
- Fragment:
  - `pbr_standard` (default)
  - `pbr_full` (advanced features)

**Lighting**:
- Forward+ clustered lighting
- Fragment shader iterates only lights in its cluster

**State**:
- Depth test: EQUAL or LESS_EQUAL
- Depth write: OFF

---

## 5. Sky / Atmosphere

**Purpose**:
- Render background lighting

**Implementation**:
- Fullscreen triangle or procedural sky

**State**:
- Depth test: LESS_EQUAL
- Depth write: OFF

---

## 6. Transparent Pass

**Purpose**:
- Glass, particles, foliage

**Shaders**:
- Task: frustum-only (no HZB)
- Mesh: full
- Fragment: usually `pbr_full`

**State**:
- Depth test: LESS_EQUAL
- Depth write: OFF
- Blending: ON

**Notes**:
- Sorted back-to-front
- No occlusion culling

---

## 7. Shadow Passes

### Directional Lights (CSM)

- Task: frustum-only
- Mesh: minimal
- Fragment: depth-only

### Spot / Point Lights

- Similar setup
- Multiple views / cube maps

**Notes**:
- No HZB
- No lighting

---

## 8. Post Processing

**Typical Chain**:
1. HDR resolve
2. Tone mapping
3. Bloom
4. Exposure
5. Color grading
6. Anti-aliasing (FXAA / TAA)

All fullscreen passes.

---

## 9. Debug Passes

**Purpose**:
- Visualization and validation

**Examples**:
- Meshlet IDs
- Cluster heatmap
- Light influence
- Normals / depth
- HZB visualization

**Rules**:
- Force `pbr_full`
- Disable shader stripping

---

## 10. UI

**Purpose**:
- ImGui / HUD

**State**:
- No depth
- Alpha blended

---

## Shader Variant Rules

- Compile-time variants via `-D` defines
- Default = standard (cheap)
- Full variant used when material or debug requires it

**Example feature macros**:
- `PBR_ENABLE_DEBUG`
- `PBR_ENABLE_TRANSMISSION`
- `PBR_ENABLE_CLEARCOAT`
- `PBR_ENABLE_ANISOTROPY`
- `PBR_ENABLE_IRIDESCENCE`
- `PBR_ENABLE_SPEC_GLOSS`

---

## Core Architectural Principles

1. Passes choose geometry + culling shaders
2. Materials choose fragment shader variants
3. Lights are clustered, never globally looped
4. Debug modes override optimization

---

## Result

This architecture is:
- Scalable
- GPU-driven
- Variant-friendly
- Debuggable
- Production-grade for desktop engines

---

**End of Document**


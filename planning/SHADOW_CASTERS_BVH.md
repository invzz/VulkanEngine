# Shadow Caster Culling: From CPU BVH to GPU-Driven

This document describes how to add proper shadow-caster culling to the engine, starting from the current `ShadowSystem` code and progressing through three levels of sophistication.

---

## Current State

### Shadow rendering flow

1. `ShadowSystem::renderShadowMaps()` iterates **all** entities with `ModelComponent + TransformComponent`.
2. For each shadow map (cascade or spot), it calls `renderToShadowMap()` which loops over the same full entity list.
3. No frustum culling is performed — every shadow caster is drawn into every shadow map.

```cpp
// src/Engine/Systems/ShadowSystem.cpp — current loop
auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
for (auto entity : view)
{
    // ... draw to shadow map
}
```

### Problems

| Issue | Impact |
|-------|--------|
| No per-cascade culling | Wastes GPU time drawing objects outside the cascade |
| No AABB data on Model | Can't cull without bounds |
| Camera frustum never involved | ✅ Correct for shadows, but no light-frustum test either |

---

## Prerequisites: Add AABB to Model

Before any culling, each `Model` needs a bounding box.

### 1. Store AABB in Model

```cpp
// include/Engine/Resources/Model.hpp
struct AABB {
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    void expand(const glm::vec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
    glm::vec3 center() const { return 0.5f * (min + max); }
    glm::vec3 extents() const { return 0.5f * (max - min); }
};

class Model {
    // ...
    AABB localBounds_;  // object-space AABB
public:
    const AABB& getLocalBounds() const { return localBounds_; }
};
```

### 2. Compute during load

In the vertex loading path (`Model::createFromVertices` or GLTF importer), accumulate:

```cpp
for (const auto& v : vertices) {
    localBounds_.expand(v.position);
}
```

### 3. World-space AABB helper

```cpp
inline AABB transformAABB(const AABB& local, const glm::mat4& world) {
    glm::vec3 corners[8];
    corners[0] = glm::vec3(world * glm::vec4(local.min.x, local.min.y, local.min.z, 1));
    corners[1] = glm::vec3(world * glm::vec4(local.max.x, local.min.y, local.min.z, 1));
    // ... all 8 corners
    AABB result;
    for (auto& c : corners) result.expand(c);
    return result;
}
```

---

## Level 1 — CPU Light-Frustum Culling (Cheap Win) ✅ COMPLETED

### Goal

For each shadow map, test each shadow caster's world AABB against the light frustum. Skip draws that are guaranteed invisible.

### Frustum representation

```cpp
struct Frustum {
    glm::vec4 planes[6]; // ax+by+cz+d form, normal pointing inward
};
```

For an orthographic directional cascade:

```cpp
Frustum extractOrthoFrustum(const glm::mat4& lightVP) {
    Frustum f;
    // left, right, bottom, top, near, far
    f.planes[0] = lightVP[3] + lightVP[0];
    f.planes[1] = lightVP[3] - lightVP[0];
    f.planes[2] = lightVP[3] + lightVP[1];
    f.planes[3] = lightVP[3] - lightVP[1];
    f.planes[4] = lightVP[3] + lightVP[2];
    f.planes[5] = lightVP[3] - lightVP[2];
    for (auto& p : f.planes) p /= glm::length(glm::vec3(p));
    return f;
}
```

### AABB vs Frustum test

```cpp
bool aabbInFrustum(const AABB& box, const Frustum& f) {
    for (int i = 0; i < 6; i++) {
        glm::vec3 p = box.min;
        if (f.planes[i].x >= 0) p.x = box.max.x;
        if (f.planes[i].y >= 0) p.y = box.max.y;
        if (f.planes[i].z >= 0) p.z = box.max.z;
        if (glm::dot(glm::vec3(f.planes[i]), p) + f.planes[i].w < 0)
            return false;
    }
    return true;
}
```

### Integration into ShadowSystem

```cpp
void ShadowSystem::renderToShadowMap(FrameInfo& frameInfo, ShadowMap& shadowMap,
                                      const glm::mat4& lightSpaceMatrix)
{
    Frustum lightFrustum = extractOrthoFrustum(lightSpaceMatrix);

    shadowMap.beginRenderPass(frameInfo.commandBuffer);
    pipeline_->bind(frameInfo.commandBuffer);

    auto view = frameInfo.scene->getRegistry().view<ModelComponent, TransformComponent>();
    for (auto entity : view)
    {
        auto [modelComp, transform] = view.get<ModelComponent, TransformComponent>(entity);
        if (!modelComp.model) continue;

        // CPU cull against light frustum
        AABB worldAABB = transformAABB(modelComp.model->getLocalBounds(),
                                        transform.modelTransform());
        if (!aabbInFrustum(worldAABB, lightFrustum)) continue;

        // Draw
        ShadowPushConstants push{};
        push.modelMatrix = transform.modelTransform();
        push.lightSpaceMatrix = lightSpaceMatrix;
        vkCmdPushConstants(...);
        modelComp.model->bind(frameInfo.commandBuffer);
        modelComp.model->draw(frameInfo.commandBuffer);
    }

    ShadowMap::endRenderPass(frameInfo.commandBuffer);
}
```

### Expected gains

- Reduces draw calls per cascade by rejecting off-screen casters.
- Complexity: O(N × cascades) frustum tests per frame (N = caster count).
- Works well for < 5k objects.

---

## Level 1.5 — CPU BVH (Optional)

If you have many static objects, build a BVH once and query per cascade:

```cpp
class ShadowCasterBVH {
public:
    void build(const std::vector<ShadowCaster>& casters);
    void query(const Frustum& f, std::vector<uint32_t>& outIndices) const;
};
```

- Reduces per-cascade cost from O(N) to O(log N + k).
- Rebuild or refit when objects move.
- Good for large static worlds (Sponza, cities).

---

## Level 2 — GPU Shadow Caster Culling (Big Win) ✅ COMPLETED

Move frustum testing to a compute shader. The CPU uploads caster AABBs once; the GPU culls per cascade in parallel.

### GPU data structures

```cpp
// CPU side
struct GPUShadowCaster {
    glm::mat4 worldMatrix;
    glm::vec4 aabbMin; // w unused
    glm::vec4 aabbMax;
};
```

```glsl
// shadow_cull.comp
layout(set = 0, binding = 0) readonly buffer Casters {
    GPUShadowCaster casters[];
};

layout(set = 0, binding = 1) writeonly buffer VisibleIndices {
    uint visibleCount;
    uint indices[];
};

layout(push_constant) uniform PC {
    mat4 lightVP;
};

layout(local_size_x = 64) in;

bool aabbInFrustum(vec3 bmin, vec3 bmax, mat4 vp);

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= casters.length()) return;

    GPUShadowCaster c = casters[idx];
    // Transform AABB to world (already stored in world coords ideally)
    if (aabbInFrustum(c.aabbMin.xyz, c.aabbMax.xyz, lightVP)) {
        uint slot = atomicAdd(visibleCount, 1);
        indices[slot] = idx;
    }
}
```

### Indirect draw buffer

After culling, use `vkCmdDrawIndexedIndirect` or `vkCmdDrawIndirect`:

```cpp
struct VkDrawIndexedIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
};
```

The compute shader (or a second pass) writes one command per visible caster.

### Render flow

```
1. Upload caster SSBOs (once or when scene changes)
2. For each cascade:
   a. Clear visibleCount to 0
   b. Dispatch shadow_cull.comp with cascade lightVP
   c. Memory barrier (compute → indirect)
   d. vkCmdDrawIndexedIndirect(indirectBuffer, ...)
3. Done — no CPU readback
```

### Expected gains

- Culling cost: ~0.1–0.3 ms per cascade on modern GPUs.
- Scales to 100k+ objects.
- Completely camera-independent.

---

## Level 3 — GPU-Driven Everything (Advanced) ✅ COMPLETED

At this level you're doing:

- GPU frustum culling
- GPU LOD selection
- GPU draw call generation (multi-draw-indirect)
- Mesh shaders (optional)

### Key additions

| Feature | Benefit |
|---------|---------|
| Persistent caster SSBO | No per-frame upload |
| Hierarchical Z (HiZ) occlusion | Skip occluded casters |
| Mesh shaders | Combine cull + draw in one dispatch |
| Two-phase culling | Coarse (object) + fine (meshlet) |

### Mesh shader shadow path

```glsl
// shadow.task
taskPayloadSharedEXT MeshletPayload payload;

void main() {
    // Cull meshlets against light frustum
    // Write visible meshlet indices to payload
    EmitMeshTasksEXT(visibleCount, 1, 1);
}

// shadow.mesh
void main() {
    // Fetch meshlet, emit triangles
}
```

This is the most efficient path but requires `VK_EXT_mesh_shader`.

---

## Migration Path

| Step | Effort | Gain |
|------|--------|------|
| Add AABB to Model | Small | Enables all culling |
| Level 1 CPU cull | Small | Immediate draw reduction |
| Level 1.5 BVH | Medium | Better for large static scenes |
| Level 2 GPU cull | Medium | Massive scale, no CPU bottleneck |
| Level 3 mesh shaders | Large | Ultimate performance |
| HiZ occlusion | Large | Handles dense overlapping geometry |

---

## Files to Modify

| File | Changes |
|------|---------|
| `include/Engine/Resources/Model.hpp` | Add `AABB localBounds_` and getter |
| `src/Engine/Resources/Model.cpp` | Compute AABB during vertex load |
| `src/EngineImporters/Resources/importers/GLTFImporter.cpp` | Compute AABB during GLTF load |
| `include/Engine/Systems/ShadowSystem.hpp` | Add `Frustum` struct, optional BVH |
| `src/Engine/Systems/ShadowSystem.cpp` | Add frustum extraction + cull in `renderToShadowMap` |
| `assets/shaders/shadow_cull.comp` | (Level 2) GPU cull compute shader |
| `src/Engine/Systems/ShadowSystem.cpp` | (Level 2) Dispatch compute, indirect draw |

---

## Summary

| Level | Where | Complexity | Performance | Status |
|-------|-------|------------|-------------|--------|
| 1 | CPU | Low | Good for < 5k objects | ✅ Done |
| 1.5 | CPU + BVH | Medium | Good for static worlds | Skipped |
| 2 | GPU compute | Medium | Scales to 100k+ | ✅ Done |
| 3 | GPU mesh shaders | High | Maximum efficiency | Pending |

Start with **Level 1** to prove the architecture (shadow visibility ≠ camera visibility), then upgrade as needed based on profiling.

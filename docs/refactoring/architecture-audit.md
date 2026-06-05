# Architecture Audit — Issues & Proposed Changes

**Date:** 2026-06-04
**Status:** Draft
**Scope:** Full codebase review (Engine, Editor, ModelLib, EngineSceneIO)

---

## Codebase Summary

| Metric | Count |
|--------|-------|
| Headers (.hpp) | 168 |
| Source files (.cpp) | 129 |
| Total LOC (src) | ~15,000+ |
| Libraries | Engine, ModelLib, EngineSceneIO, stb_provider |
| Binaries | Editor, IBLBaker |
| Architecture | Clean Architecture (Ports/Adapters/UseCases) + ECS (entt) |
| Build | xmake, C++20 |

---

## Issues (Ranked by Impact)

### Issue 1: EngineState is a God Object

**Severity:** High
**Files:** `include/Engine/EngineState.hpp` (376 lines), `src/Engine/EngineState.cpp` (258 lines)

**Current state:**
- 16 system `unique_ptr` members
- 4 descriptor pool + 4 layout + 4 descriptor set vectors (12 Vulkan resource containers)
- 2 input device pointers
- Scene + skybox + 8 bool flags + 2 settings structs
- ~30 accessor methods with `Ref` suffixes
- 6 state service factories + 6 state view builders
- Friend declarations to 6 state services

**Problems:**
1. Every change to rendering state, descriptor management, or runtime settings requires touching EngineState
2. Header pulls in 16 concrete system headers — any system change triggers recompilation of all dependents
3. The class knows too much about too many domains (Vulkan descriptors, physics, UI toggles, scene state)
4. State views are bags of raw pointers with no validation

**Proposed change:** Split into domain-scoped sub-objects

```
EngineState (thin coordinator, ~80 lines)
├── GraphicsState          → owns all render/physics systems
├── DescriptorManager      → owns pools, layouts, per-frame descriptor sets
├── SceneRuntime           → owns Scene, selectedEntity, cameraEntity
└── RuntimeSettings        → owns bool flags, SkyboxSettings, ShadowSettings
```

**Benefits:**
- Reduced header dependencies — systems no longer visible at EngineState boundary
- Each sub-object has a focused interface
- Non-breaking: public API stays the same, internals reorganized

---

### Issue 2: Port/Adapter Pattern Over-Engineered

**Severity:** High
**Files:** 16+ port interfaces, 16+ adapter implementations, 9+ use cases

**Current state:**
Most adapters are trivial 20-line pass-throughs. Example:

```cpp
// SceneEntityAdapter — 23 lines total
class SceneEntityAdapter final : public ISceneEntityPort {
    entt::entity createEntity() override {
        return engineState_.scene.createEntity();
    }
    // ... 3 more methods, all pass-through
};
```

**Problems:**
1. Clean Architecture justifies itself when you have multiple implementations of a port. You have one client (Editor).
2. ~600 lines of boilerplate across adapters, ports, and use cases with zero real abstraction benefit
3. Call graph is 3-4 indirections deep: `UI → UseCase → Port → Adapter → EngineState → system`
4. Debugging requires stepping through 4 layers for a simple operation

**Proposed change:** Collapse trivial adapters to direct calls

**Keep the pattern where it provides real value:**
- `IScenePersistencePort` — serialization is a natural boundary
- `IRenderContextPort` — Vulkan resource abstraction

**Collapse (trivial pass-throughs):**
- SceneEntity, Camera, Transform, SceneSettings, PhysicsRuntime, EnvironmentLighting,
  PostProcessingAccess, Composition, AnimationAccess, DescriptorAccess, RuntimeState,
  SceneRuntimeAccess, SkyboxAccess

**Replace with direct methods on EngineState or EngineFacade:**

```cpp
// Before:
sceneEntityManagementUseCase->execute(EntityType::Model, {0,0,0});

// After:
engineState.scene().createModelEntity({0,0,0});
```

**Estimated reduction:** ~30 files, ~1500 lines

---

### Issue 3: FrameInfo is a God Struct

**Severity:** Medium
**Files:** `include/Engine/Graphics/FrameInfo.hpp`

**Current state:**
14+ fields mixing frame data, camera ref, scene ptr, selection state, debug mode, and system pointers.

```cpp
struct FrameInfo {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera& camera;
    VkDescriptorSet globalDescriptorSet;
    VkDescriptorSet globalTextureSet;
    Scene* scene;
    uint32_t selectedObjectId;
    entt::entity selectedEntity;
    entt::entity cameraEntity;
    MorphTargetManager* morphManager;
    VkExtent2D extent;
    int debugMode;
    ModelRenderSystem* modelRenderSystem;
    ShadowSystem* shadowSystem;
};
```

**Problems:**
1. Passes that only need `frameIndex + commandBuffer` receive everything
2. Adding a new field requires touching every FrameInfo construction site
3. System pointers in FrameInfo create circular dependency between pass and system

**Proposed change:** Split into core + optional context

```cpp
struct FrameContext {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    VkExtent2D extent;
};

// Passes receive FrameContext + specific references they actually need
// OR: FrameInfo becomes a class with lazy getters
```

---

### Issue 4: ModelRenderSystem is 1096 Lines

**Severity:** Medium
**Files:** `src/Engine/Systems/ModelRenderSystem.cpp`

**Current state:**
Single file handles:
- CPU frustum culling (~90 lines)
- Material merging (~100 lines)
- G-buffer rendering
- Transmission pass
- Alpha-blend pass
- Multithreaded secondary command buffer recording
- LOD evaluation

**Problems:**
1. Violates single responsibility — 5+ concerns in one file
2. Frustum culling logic is generic and reusable but embedded in render loop
3. Material merging is pure data transformation, not tied to rendering
4. Hard to test individual concerns in isolation

**Proposed change:** Extract into focused sub-classes

```
ModelRenderSystem (orchestrator, ~300 lines)
├── FrustumCuller        (standalone, ~100 lines)
├── MaterialMerger       (standalone, ~80 lines)
├── GBufferRenderer      (~200 lines)
└── TransparentRenderer  (transmission + alpha-blend, ~200 lines)
```

Multithreaded recording stays in ModelRenderSystem — it's orchestration.

---

### Issue 5: Descriptor Management is Scattered

**Severity:** Medium
**Files:** EngineState.hpp/cpp, OffscreenPass.cpp, multiple adapters

**Current state:**
- Descriptor pools/layouts live in EngineState
- Per-frame updates happen in OffscreenPass
- Gbuffer descriptor writes in OffscreenPass::refreshGbufferDescriptors()
- Shadow descriptor updates in a freestanding function in OffscreenPass.cpp
- Post-process descriptors managed separately

**Problems:**
1. Descriptor lifecycle split across 3+ files
2. Adding a new descriptor set requires changes in EngineState, the pass, and adapter
3. No single place to understand descriptor allocation strategy

**Proposed change:** Centralize in DescriptorManager

```cpp
class DescriptorManager {
    // Owns all pools, layouts, per-frame sets
    void updateGbufferDescriptors(int frame, Renderer&);
    void updateShadowDescriptors(int frame, ShadowSystem&);
    void updateDeferredDescriptors(int frame);
    // Frame-scoped accessors
    VkDescriptorSet gbufferSet(int frame);
    VkDescriptorSet shadowSet(int frame);
};
```

Passes call `descriptorManager.updateForFrame(frameIndex)` at frame start.

---

### Issue 6: StateViews are Bags of Raw Pointers

**Severity:** Low
**Files:** `include/Engine/Application/StateViews/RenderingStateView.hpp`

**Current state:**
```cpp
struct RenderingStateView {
    ModelRenderSystem* modelRenderSystem = nullptr;
    ShadowSystem* shadowSystem = nullptr;
    // ... 15 more raw pointers
};
```

**Problems:**
1. Null dereference is silent — no validation
2. No way to know if a view is valid before use

**Proposed change:** Add validation method

```cpp
struct RenderingStateView {
    // ... same fields
    void validate() const {
        assert(modelRenderSystem != nullptr && "modelRenderSystem not initialized");
        assert(shadowSystem != nullptr && "shadowSystem not initialized");
    }
};
```

---

### Issue 7: Pass Construction is Wiring Spaghetti

**Severity:** Low
**Files:** `src/Editor/app.cpp` (setupRenderGraph)

**Current state:**
30+ lines creating adapters, building state views, constructing passes with 5-6 parameters each.

**Problems:**
1. Every new pass requires understanding the adapter ecosystem
2. Hard to read and modify
3. Adapter lifetime management scattered

**Proposed change:** Passes take `EngineState&` and access what they need internally

```cpp
void App::setupRenderGraph() {
    auto graph = std::make_unique<RenderGraph>();
    auto& es = engineState;
    
    graph->addPass(std::make_unique<UpdatePass>(es, renderer));
    graph->addPass(std::make_unique<ComputePass>(es));
    graph->addPass(std::make_unique<ShadowPass>(es, *renderContext));
    graph->addPass(std::make_unique<DepthPrepass>(es, renderer));
    graph->addPass(std::make_unique<OffscreenPass>(es, renderer, device, debugMode));
    graph->addPass(std::make_unique<CompositionPass>(es, renderer, *camera, window));
    
    renderPipeline->setRenderGraph(std::move(graph));
}
```

---

## Summary Matrix

| # | Change | Effort | Impact | Risk | Prerequisites |
|---|--------|--------|--------|------|---------------|
| 1 | Split EngineState into sub-objects | Medium | High | Low | None |
| 2 | Collapse trivial port/adapter boilerplate | Large | High | Medium | #1 |
| 3 | Split FrameInfo into core + optional | Small | Medium | Low | None |
| 4 | Split ModelRenderSystem | Medium | Medium | Low | None |
| 5 | Centralize descriptor management | Medium | Medium | Low | None |
| 6 | Add StateView validation | Small | Low | None | None |
| 7 | Simplify pass construction | Small | Low | None | None |

## Recommended Implementation Order

### Phase A: Quick Wins (no structural change)
- #6: Add StateView validation
- #7: Simplify pass construction

### Phase B: Foundation for Bigger Changes
- #3: FrameInfo simplification
- #5: Centralize descriptor management

### Phase C: Core Restructuring
- #1: Split EngineState into sub-objects
- #2: Collapse trivial adapters (biggest payoff, requires #1 first)

### Phase D: Code Quality
- #4: Split ModelRenderSystem (can be done anytime, independent)

---

## Relationship to Existing Clean Architecture Migration

The existing `clean-architecture-migration-v2.md` plan addresses Clean Architecture
boundary enforcement. This audit identifies broader issues that affect developer
experience regardless of architectural pattern. Some overlap:

- **Issue #1 (EngineState split)** complements the existing plan's Step 4
- **Issue #2 (adapter collapse)** is a counter-proposal to the existing plan —
  the current plan adds more adapters; this audit argues for fewer
- **Issues #3-7** are orthogonal to Clean Architecture and address code
  organization regardless of pattern

The key philosophical difference: the existing plan doubles down on Clean
Architecture indirection. This audit argues that with a single client (Editor),
the indirection cost outweighs the benefit for most ports.

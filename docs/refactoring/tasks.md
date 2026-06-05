# Refactoring Task Breakdown

Each task is designed to be completed in a single session, verified with a build, and committed independently.

---

## Task 0-RECO: Baseline Verification

**Goal:** Confirm current state before changes.

- [ ] Run `xmake build` — verify clean build
- [ ] Run `xmake run` briefly — verify editor launches
- [ ] Run existing tests — verify all pass
- [ ] Count current files: `find src include -name "*.cpp" -o -name "*.hpp" | wc -l`
- [x] Document baseline metrics in this file
- [x] Create DescriptorManager class (header + implementation)
- [x] Integrate DescriptorManager into EngineState (initDescriptorResources, allocatePerFrameDescriptorSets, initPostProcessing)
- [x] Update all EngineState descriptor accessors to delegate to descriptorManager
- [x] Build and test verification (604 tests pass)

**Baseline metrics (2026-06-04):**
- Headers: 168 | Source files: 129 | Total LOC (src): ~15,000
- Largest file: ModelRenderSystem.cpp (1096 lines)
- Port headers: 17 | Adapter headers: 17 | Adapter sources: 17 | UseCase headers: 16
- Direct `engineState.` calls in app.cpp: 14
- All 604 tests pass (3 skipped)

**Estimated effort:** 10 min
**Verification:** Build succeeds, tests pass

---

## Task 6-STATEVIEW-VALIDATE: Add StateView Validation

**Goal:** Add `validate()` methods to state view structs to catch null dereferences early.

**Files to modify:**
- `include/Engine/Application/StateViews/RenderingStateView.hpp`
- `include/Engine/Application/StateViews/SceneRuntimeStateView.hpp`
- `include/Engine/Application/StateViews/InputStateView.hpp`
- `include/Engine/State/StateViews.hpp` (ResourceStateView, SystemServicesView)

**Changes:**
1. Add `[[nodiscard]] bool isValid() const` method to each struct
2. Method checks all non-nullable pointers are non-null
3. Call `validate()` in each state service's `view()` method

**Example:**
```cpp
// RenderingStateView.hpp
[[nodiscard]] bool isValid() const {
    return modelRenderSystem != nullptr
        && shadowSystem != nullptr
        && renderContextPort != nullptr;
    // Nullable fields (showSkybox, debugMode) excluded — they may legitimately be null
}
```

**Estimated effort:** 30 min
**Verification:** `xmake build` succeeds, no behavior change

---

## Task 7-PASS-SIMPLIFY: Simplify Pass Construction

**Goal:** Reduce constructor parameter count on render passes by having them take `EngineState&` instead of state views + adapters.

**Phase 7a: Create EngineFacade (new file)**

- [ ] Create `include/Engine/EngineFacade.hpp` + `src/Engine/EngineFacade.cpp`
- [ ] Facade wraps `EngineState&` and provides convenient getters:
  ```cpp
  class EngineFacade {
      EngineState& state;
   public:
      ModelRenderSystem& modelRender() { /* ... */ }
      ShadowSystem& shadow() { /* ... */ }
      Scene& scene() { return state.sceneRef(); }
      bool showGrid() const { return state.showGridRef(); }
      // ... etc
  };
  ```

**Phase 7b: Update one pass as proof of concept**

- [ ] Update `OffscreenPass` constructor to take `EngineFacade&` instead of
  `(RenderingStateView, IDescriptorAccessPort&, IRuntimeStatePort&, Device&, int&)`
- [ ] Update `app.cpp::setupRenderGraph()` to construct pass with facade
- [ ] Verify build succeeds

**Phase 7c: Update remaining passes**

- [ ] Update `CompositionPass`
- [ ] Update `ShadowPass`
- [ ] Update `DepthPrepass`
- [ ] Update `UpdatePass`
- [ ] Update `ComputePass`

**Estimated effort:**
- 7a: 45 min
- 7b: 30 min
- 7c: 45 min
- Total: ~2.25 hours

**Verification:** `xmake build` succeeds, editor runs, rendering unchanged

---

## Task 3-FRAMEINFO: Split FrameInfo into Core + Optional

**Goal:** Reduce FrameInfo from 15 fields to ~6 core fields, making it clear what each pass actually needs.

**Files to modify:**
- `include/Engine/Graphics/FrameInfo.hpp`
- `src/Editor/app.cpp` (render() method)
- All pass files that use FrameInfo fields

**Changes:**

1. Split FrameInfo:
```cpp
struct FrameContext {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    VkExtent2D extent;
};

struct FrameInfo {
    FrameContext ctx;
    Camera& camera;
    Scene& scene;
    VkDescriptorSet globalDescriptorSet;
    VkDescriptorSet globalTextureSet;
    
    // Optional — accessed via getter, not stored
    MorphTargetManager* morphManager{nullptr};
    int debugMode{0};
};
```

2. Remove system pointers from FrameInfo (`modelRenderSystem`, `shadowSystem`) —
   passes should get these from EngineState/facade instead

3. Update all pass `execute()` methods to use `frameInfo.ctx.frameIndex` etc.

**Estimated effort:** 1.5 hours
**Verification:** `xmake build` succeeds, rendering visually identical

---

## Task 5-DESCRIPTOR: Centralize Descriptor Management

**Goal:** Move all descriptor pool/layout/set management from EngineState into a dedicated DescriptorManager.

**Files to modify:**
- New: `include/Engine/Graphics/DescriptorManager.hpp` + `src/Engine/Graphics/DescriptorManager.cpp`
- `include/Engine/EngineState.hpp` (move members)
- `src/Engine/EngineState.cpp` (move initialization)
- `src/Engine/Graphics/Passes/OffscreenPass.cpp` (update calls)
- `include/Editor/Infrastructure/DescriptorAccessAdapter.hpp` (update to use DescriptorManager)

**Changes:**

1. Create DescriptorManager class:
```cpp
class DescriptorManager {
public:
    void initialize(Device& device, Renderer& renderer);
    void allocatePerFrameSets(Renderer& renderer);
    
    // Per-frame updates
    void updateGbufferDescriptors(int frame, Renderer&);
    void updateShadowDescriptors(int frame, ShadowSystem&, Device&);
    void updatePostProcessDescriptors(int frame, Renderer&);
    
    // Accessors
    VkDescriptorSet gbufferSet(int frame) const;
    VkDescriptorSet deferredShadowSet(int frame) const;
    VkDescriptorSet deferredIblSet(int frame) const;
    VkDescriptorSet postProcessSet(int frame) const;
    // ... pool/layout refs
};
```

2. Move descriptor members from EngineState to DescriptorManager

3. Update EngineState to own DescriptorManager

4. Update OffscreenPass to call DescriptorManager instead of scattered logic

5. Update DescriptorAccessAdapter to delegate to DescriptorManager

**Estimated effort:** 2 hours
**Verification:** `xmake build` succeeds, rendering identical, no descriptor leaks

---

## Task 1-ENGSTATE-SPLIT: Split EngineState into Sub-Objects

**Goal:** Break the 376-line EngineState into focused sub-objects.

**Prerequisites:** Task 5-DESCRIPTOR (descriptor management already extracted)

**Files to modify:**
- New: `include/Engine/Graphics/GraphicsState.hpp` + `.cpp`
- New: `include/Engine/Scene/SceneRuntime.hpp` + `.cpp`
- New: `include/Engine/RuntimeSettings.hpp` + `.cpp`
- `include/Engine/EngineState.hpp` (drastically reduced)
- `src/Engine/EngineState.cpp` (delegates to sub-objects)
- All state services (update to use sub-objects)
- All adapters (update references)

**Changes:**

1. Create GraphicsState — owns all system pointers:
```cpp
class GraphicsState {
    std::unique_ptr<ModelRenderSystem> modelRenderSystem;
    std::unique_ptr<ShadowSystem> shadowSystem;
    // ... all other systems
    void initialize(Device&, Renderer&, ...);
};
```

2. Create SceneRuntime — owns scene + selection state:
```cpp
class SceneRuntime {
    Scene scene;
    entt::entity selectedEntity = entt::null;
    entt::entity cameraEntity = entt::null;
};
```

3. Create RuntimeSettings — owns flags + settings structs:
```cpp
class RuntimeSettings {
    bool showSkybox = false;
    bool showGrid = false;
    bool debugMode = false;
    ShadowSettings shadowSettings;
    SkyboxSettings skySettings;
    PostProcessPushConstants postProcessPush;
};
```

4. Reduce EngineState to thin coordinator:
```cpp
class EngineState {
    std::unique_ptr<GraphicsState> graphics;
    std::unique_ptr<DescriptorManager> descriptors;
    std::unique_ptr<SceneRuntime> sceneRuntime;
    std::unique_ptr<RuntimeSettings> settings;
    // ... state services delegate to sub-objects
};
```

5. Update all state services to access sub-objects

6. Update all adapters to access sub-objects

**Estimated effort:** 3-4 hours
**Verification:** `xmake build` succeeds, all tests pass, editor runs

---

## Task 2-ADAPTER-COLLAPSE: Collapse Trivial Adapters

**Goal:** Remove trivial pass-through adapters and replace with direct EngineState/facade calls.

**Prerequisites:** Task 1-ENGSTATE-SPLIT (EngineState must be well-structured first)

**Files to remove:**
- `include/Editor/Infrastructure/SceneEntityAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/CameraAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/TransformAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/SceneSettingsAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/PhysicsRuntimeAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/EnvironmentLightingAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/PostProcessingAccessAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/CompositionAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/AnimationAccessAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/DescriptorAccessAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/RuntimeStateAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/SceneRuntimeAccessAdapter.hpp` + `.cpp`
- `include/Editor/Infrastructure/SkyboxAccessAdapter.hpp` + `.cpp`

**Files to remove (ports):**
- Corresponding port headers in `include/Engine/Application/Ports/`

**Files to remove (use cases):**
- Trivial use cases that just delegate to one port

**Files to modify:**
- `include/Editor/app.hpp` (remove adapter/usecase members)
- `src/Editor/app.cpp` (replace adapter/usecase calls with direct calls)
- `include/Editor/ui/*.hpp` + `.cpp` (update panel constructors)

**Approach per adapter (repeat 13 times):**
1. Identify the adapter and its port
2. Identify all callers of the port
3. Replace port calls with direct EngineState/facade calls
4. Remove use case (if trivial)
5. Remove adapter + port files
6. Verify build

**Estimated effort:** 4-5 hours (largest task)
**Verification:** `xmake build` succeeds, editor runs, all functionality preserved

---

## Task 4-MODELRENDER-SPLIT: Split ModelRenderSystem

**Goal:** Extract frustum culling, material merging, and transparent rendering from the 1096-line ModelRenderSystem.

**Files to modify:**
- New: `include/Engine/Graphics/FrustumCuller.hpp` + `.cpp`
- New: `include/Engine/Scene/MaterialMerger.hpp` + `.cpp`
- `src/Engine/Systems/ModelRenderSystem.cpp` (extract code)
- `include/Engine/Systems/ModelRenderSystem.hpp` (update interface)

**Changes:**

1. Extract FrustumCuller:
   - Move `Frustum` struct, `extractFrustumFromMatrix`, `aabbInFrustum`, `isEntityVisible`
   - Make standalone, testable utility

2. Extract MaterialMerger:
   - Move `isMeaningfulMaterialOverride`, `mergeMaterialOverrides`
   - Pure data transformation — no Vulkan dependencies

3. Extract TransparentRenderer:
   - Move transmission + alpha-blend rendering logic
   - ModelRenderSystem delegates to it

4. ModelRenderSystem becomes orchestrator (~300 lines)

**Estimated effort:** 2 hours
**Verification:** `xmake build` succeeds, rendering identical

---

## Execution Order & Dependencies

```
Task 0-RECO
    │
    ├── Task 6-STATEVIEW-VALIDATE    (independent, quick win)
    │
    ├── Task 7-PASS-SIMPLIFY         (independent, but benefits from Task 1)
    │
    ├── Task 3-FRAMEINFO             (independent)
    │
    ├── Task 5-DESCRIPTOR            (independent)
    │       │
    │       ▼
    │   Task 1-ENGSTATE-SPLIT        (depends on Task 5)
    │       │
    │       ▼
    │   Task 2-ADAPTER-COLLAPSE      (depends on Task 1)
    │
    └── Task 4-MODELRENDER-SPLIT     (independent, can be done anytime)
``

**Recommended sequence for first pass:**
1. Task 0-RECO → baseline
2. Task 6-STATEVIEW-VALIDATE → quick win, builds confidence
3. Task 3-FRAMEINFO → reduces FrameInfo bloat
4. Task 5-DESCRIPTOR → centralizes Vulkan state
5. Task 1-ENGSTATE-SPLIT → core restructuring
6. Task 2-ADAPTER-COLLAPSE → biggest reduction
7. Task 4-MODELRENDER-SPLIT → code quality

**Total estimated effort:** ~12-15 hours across 7 tasks

---

## Per-Task Checklist Template

For each task, verify:

- [ ] `xmake build` succeeds
- [ ] Editor launches and renders scene
- [ ] Existing tests pass
- [ ] No new compiler warnings
- [ ] Commit with descriptive message
- [ ] Update this file with completion status

# Clean Architecture Migration - Implementation Plan v3

**Date:** 2026-06-03
**Status:** Phase 3 Complete, Phase 4/5 Pending
**Previous plan:** clean-architecture-migration-v2.md (Superseded)

---

## Status Summary

| Phase | Status | Tests Passing |
|-------|--------|---------------|
| Phase 1: Core Decoupling | ✅ Complete | 610/613 (3 skipped) |
| Phase 2: Render Pass Decoupling | ✅ Complete | 610/613 (3 skipped) |
| Phase 3: UI Panel Decoupling | ✅ Complete | 610/613 (3 skipped) |
| Phase 4: Domain Purification | ⏳ Pending | - |
| Phase 5: Header Isolation (Pimpl) | ⏳ Pending | - |

---

## Phase 3: UI Panel Decoupling — COMPLETE

**Goal:** Remove all `EngineState*` from UI panels.

### What Was Done

#### 3.1: Defined UI-facing Ports

Created narrow interfaces in `include/Engine/Application/Ports/`:

1. **`ISettingsPort.hpp`**
   - Settings getters/setters (`showSkybox`, `showGrid`, `showDebugObjects`)
   - ImGui pointer bindings (`showSkyboxPtr()`, `postProcessPush()`, etc.)
   - Infrastructure accessors (`renderingService()`, `resourceManager()`, `modelRenderSystem()`)

2. **`ISceneManagementPort.hpp`**
   - Scene access (`scene()`, `registry()`)
   - Resource access (`resourceManager()`)

3. **`IInspectorPort.hpp`**
   - Transform updates (`updateTransform()`, `updateTranslation()`, `updateRotation()`, `updateScale()`)
   - Entity activation (`setEntityActive()`)
   - Scene/registry access (`scene()`, `registry()`, `joltPhysicsSystem()`)

4. **`ISceneRuntimeAccessPort.hpp`**
   - Runtime state access (`showSkybox()`, `showGrid()`, `physicsSimulationRunning()`, etc.)

#### 3.2: Created UI Adapters

Created adapters in `include/Editor/Infrastructure/`:

- **`SettingsPortAdapter`** — bridges `EngineState` to `ISettingsPort`
- **`SceneManagementPortAdapter`** — bridges `EngineState` to `ISceneManagementPort`
- **`InspectorPortAdapter`** — bridges `EngineState` to `IInspectorPort`
- **`SceneRuntimeAccessAdapter`** — bridges `EngineState` to `ISceneRuntimeAccessPort`

#### 3.3: Refactored UI Panels

Modified panel constructors to accept ports via `std::shared_ptr`:

**`SettingsPanel`**
```cpp
// Before:
explicit SettingsPanel(EngineState* engineState);

// After:
SettingsPanel(std::shared_ptr<ISettingsPort> settingsPort,
              std::shared_ptr<ISceneRuntimeAccessPort> sceneRuntimePort,
              bool& multithreadedRecordingEnabled, uint32_t& multithreadedRecordingThreads, int& debugMode);
```

**`ScenePanel`**
```cpp
// Before:
explicit ScenePanel(Device& device, EngineState* engineState);

// After:
ScenePanel(Device& device, std::shared_ptr<ISceneManagementPort> scenePort);
```

**`InspectorPanel`**
```cpp
// Before:
explicit InspectorPanel(EngineState* engineState);

// After:
InspectorPanel(std::shared_ptr<IInspectorPort> inspectorPort, JoltPhysicsSystem* joltPhysicsSystem);
```

Key changes:
- `InspectorPanel` now lazy-initializes sub-panels (`TransformPanel`, `LightsPanel`, `AnimationPanel`, `PhysicsPanel`) in `render()` to avoid stale `Scene&` references
- `SettingsPanel` uses `settingsPort_->renderingService().view()` for rendering state access
- `ScenePanel` uses `scenePort_->scene()` and `scenePort_->registry()` for scene access

#### 3.4: Updated `app.cpp`

Wired up adapters in `App::setupUI()`:

```cpp
auto settingsAdapter = std::make_shared<SettingsPortAdapter>(engineState);
auto sceneAdapter = std::make_shared<SceneManagementPortAdapter>(engineState);
auto sceneRuntimeAccessAdapter = std::make_shared<SceneRuntimeAccessAdapter>(engineState);

uiManager->addPanel(std::make_unique<ScenePanel>(device, sceneAdapter));
uiManager->addPanel(std::make_unique<InspectorPanel>(
    std::make_shared<InspectorPortAdapter>(engineState),
    physicsRuntimePort->joltPhysicsSystem()));
uiManager->addPanel(std::make_unique<SettingsPanel>(
    settingsAdapter, sceneRuntimeAccessAdapter,
    multithreadedRecordingEnabled, multithreadedRecordingThreads, debugMode));
```

#### 3.5: Updated Architecture Tests

- Added `UIPanelsShouldNotDependOnEngineState` test — verifies UI panels don't include `EngineState.hpp`
- Updated `ScenePanelShouldUseSceneRuntimeServiceForSceneAccess` — now checks for port-based access
- Updated `HotPathsUseEngineStateSystemAccessorsNotDirectMembers` — SettingsPanel now uses port adapters

### Verification

```bash
# All UI panels are clean:
grep -r 'EngineState\*' include/Editor/ui/  # Empty
grep -r '#include.*EngineState.hpp' src/Editor/ui/  # Empty

# All tests pass:
xmake run Tests  # 610 passed, 3 skipped
```

---

## Phase 4: Domain Layer Purification

**Goal:** Remove all infrastructure dependencies from Domain headers.

### 4.1: Identify Domain-Bleed Files

Current violations (from architecture tests allowlist):
- `include/Engine/Scene/components/ModelComponent.hpp` — includes `ModelLib/Resources/Model.hpp`
- `include/Engine/Scene/components/LODComponent.hpp` — includes `ModelLib/Resources/Model.hpp`
- `include/Engine/Scene/components/AnimationComponent.hpp` — includes `ModelLib/Resources/Model.hpp`
- `include/Engine/Scene/SceneUtils.hpp` — includes `ModelLib/Resources/Model.hpp` + `ResourceManager.hpp`
- `include/Engine/Scene/Skybox.hpp` — includes `Engine/Graphics/Device.hpp`

### 4.2: Introduce Domain Handle Types

Create a `ModelHandle` type in `include/Engine/Scene/`:

```cpp
// ModelHandle.hpp
#pragma once
#include <cstdint>

struct ModelHandle {
    uint64_t id;
    bool operator==(const ModelHandle& other) const;
    bool operator!=(const ModelHandle& other) const;
};
```

### 4.3: Refactor Components to Use Handles

**`ModelComponent`**
```cpp
// Before:
class ModelComponent : public ComponentBase {
    std::shared_ptr<ModelLib::Resources::Model> model_;
};

// After:
class ModelComponent : public ComponentBase {
    ModelHandle modelHandle_;
};
```

**`LODComponent`**
```cpp
// Before:
class LODComponent : public ComponentBase {
    std::shared_ptr<ModelLib::Resources::Model> model_;
};

// After:
class LODComponent : public ComponentBase {
    std::vector<ModelHandle> lodModelHandles_;
};
```

### 4.4: Update Resource Management API

Introduce/update a resource port (e.g., `IModelResourcePort`) to provide handle-based access, decoupling the Domain from the concrete `ResourceManager`:

```cpp
class IModelResourcePort {
public:
    virtual ~IModelResourcePort() = default;
    virtual ModelHandle loadModel(const std::string& path) = 0;
    virtual std::shared_ptr<ModelLib::Resources::Model> getModel(ModelHandle handle) = 0;
    virtual void unloadModel(ModelHandle handle) = 0;
};
```

Implement this port in `ResourceManager` or a dedicated adapter.

### 4.5: Refactor Skybox

Move `SkyboxSettings` to Domain (it's just data). Move texture creation to Infrastructure:

```cpp
// Skybox.hpp (Domain)
struct SkyboxSettings {
    std::array<std::string, 6> texturePaths;
};

// SkyboxSystem.hpp (Infrastructure)
class SkyboxSystem {
    void createTextures(const SkyboxSettings& settings, IRenderContextPort* ctx);
};
```

### 4.6: Update Architecture Tests

- Remove all Domain files from allowlist in `DomainMustNotDependOnInfrastructure`.
- Verify: `grep -r '#include.*ModelLib' include/Engine/Scene/` returns empty.

---

## Phase 5: Header Isolation (EngineState Pimpl)

**Goal:** Make `EngineState.hpp` a lightweight header to reduce compile times.

### 5.0: Establish Baseline

Before implementation, measure the current compilation time for any change in `EngineState.hpp`. This provides a metric to verify the success of Phase 5.

### 5.1: Create EngineState::Impl

Create `include/Engine/State/EngineStateImpl.hpp`:

```cpp
#pragma once
#include "Engine/EngineState.hpp" // For forward declarations if needed
#include <memory>

// Concrete system includes (heavy headers)
#include "Engine/Graphics/ModelRenderSystem.hpp"
#include "Engine/Graphics/ShadowSystem.hpp"
#include "Engine/Scene/Scene.hpp"
// ... all other concrete system headers

class EngineState::Impl {
public:
    // All unique_ptr members move here
    std::unique_ptr<ModelRenderSystem> modelRenderSystem_;
    std::unique_ptr<ShadowSystem> shadowSystem_;
    std::unique_ptr<Scene> scene_;
    // ... all other systems
    std::unique_ptr<ResourceManager> resourceManager_;

    // All state data moves here
    bool showSkybox_ = false;
    bool showGrid_ = true;
    bool showDebugObjects_ = false;
    glm::vec3 skySettings_ = {};
    PostProcessPushConstants postProcessPush_ = {};
    // ... all other state
};
```

### 5.2: Update EngineState

```cpp
// EngineState.hpp (lightweight)
class EngineState {
private:
    std::unique_ptr<Impl> impl_;
public:
    // All methods delegate to impl_
    RenderingStateService renderingService();
    void setShowSkybox(bool val);
    bool showSkybox() const;
    // ... etc
};
```

### 5.3: Update State Services

Each state service must access `impl_` through the EngineState:

```cpp
class RenderingStateService {
    EngineState& engineState_;
public:
    RenderingStateView view() const {
        return RenderingStateView{
            .modelRenderSystem = engineState_.impl_->modelRenderSystem_.get(),
            .shadowSystem = engineState_.impl_->shadowSystem_.get(),
            // ... etc
        };
    }
};
```

### 5.4: Update All Direct impl_ Access

Search for `impl_->` in the codebase and update to use public accessor methods instead:

```cpp
// Before:
engineState_.impl_->showSkybox_ = true;

// After:
engineState_.setShowSkybox(true);
```

### 5.5: Update Architecture Tests

- Add test: `EngineStateShouldUsePimplPattern` (verifies no direct `impl_` access in non-EngineState code).

---

## Verification

### After Phase 3 (UI Decoupling) ✅
1. `grep -r 'EngineState\*' include/Editor/ui/` returns empty ✅
2. `grep -r '#include.*EngineState.hpp' src/Editor/ui/` returns empty ✅
3. All 610 tests pass ✅

### After Phase 4 (Domain Purification)
1. `grep -r '#include.*ModelLib' include/Engine/Scene/` returns empty
2. `grep -r '#include.*Engine/Graphics/Device.hpp' include/Engine/Scene/` returns empty
3. All 610 tests pass

### After Phase 5 (Header Isolation)
1. `EngineState.hpp` includes ≤5 headers (only ports and forward declarations)
2. `grep -r 'impl_->' --include="*.cpp" --include="*.hpp" | grep -v 'EngineState\.cpp' | grep -v 'EngineStateImpl\.cpp'` returns empty
3. All 610 tests pass
4. Compile time for `EngineState.hpp` change is <100ms (previously several seconds)

---

## Estimated Effort

| Phase | Files to Modify | Complexity | Risk |
|-------|----------------|------------|------|
| Phase 3 | ~15 files | Medium | Low (pure refactoring) |
| Phase 4 | ~15 files | High | Medium (API changes) |
| Phase 5 | ~20 files | Low/Med | Low (mechanical) |

---

## Dependencies Between Phases

1. **Phase 3** ✅ Complete — no blockers.
2. **Phase 4** — can start now (clean UI layer ready).
3. **Phase 5** — can start after Phase 4 (need stable domain before heavy header changes).

---

## Risks

1. **Phase 4:** `ModelHandle` changes may require updating serialization, scene files, and asset loading pipelines.
2. **Phase 5:** Pimpl pattern may introduce performance overhead in hot paths (render loop). Measure carefully.
3. **Phase 3:** ✅ Resolved — UI panels adapted cleanly with lazy initialization for InspectorPanel.

---

## Success Criteria

When all phases are complete:
- ✅ Zero `EngineState*` in UI panels
- ⏳ Zero infrastructure includes in Domain headers
- ⏳ `EngineState.hpp` is a lightweight header (≤5 includes)
- ✅ All 610 architecture tests pass
- ⏳ Compile time for `EngineState.hpp` change <100ms
- ⏳ Full Clean Architecture boundaries enforced

# Clean Architecture Migration - Implementation Plan v2

**Date:** 2026-06-03
**Status:** In Progress
**Previous plan:** clean-architecture-migration-plan.md (superseded)

---

## Current State Summary

### ✅ Completed (Phases 0-3)
- **Foundation:** 6 narrow state services, 5 state views, 17 ports, 17 adapters, 17 use cases
- **Render passes:** Fully decoupled from EngineState (test: `RenderPassesShouldNotDependOnEngineState` passes)
- **Infrastructure adapters:** Decoupled from EngineState (test: `EnvironmentLightingAdapterShouldNotDependOnEngineState` passes)
- **All 25 tests pass** (19 architecture + 6 lifecycle contracts)
- **Build succeeds**

### ❌ Remaining Work

#### Gap 1: app.cpp Direct EngineState Access (27 calls)

The `App` class still accesses `engineState` directly in several categories:

**Category A: RuntimeSettingsBindings construction (lines 121-134)**
```cpp
.showSkybox = &engineState.showSkyboxRef(),
.showGrid = &engineState.showGridRef(),
...
.iblSystem = rendering.iblSystem,
.modelRenderSystem = rendering.modelRenderSystem,
```
**Root cause:** `RuntimeSettingsBindingService` exists but is NOT used. app.cpp constructs `RuntimeSettingsBindings` directly.

**Category B: UI Panel construction (lines 225-229)**
```cpp
uiManager->addPanel(std::make_unique<ScenePanel>(device, &engineState));
uiManager->addPanel(std::make_unique<InspectorPanel>(
    *engineState.sceneRuntimeService().view().scene,
    &engineState.physicsSimulationRunningRef(),
    &engineState.showColliderWireframesRef(),
    &engineState.solidGroundEnabledRef(),
    engineState.getJoltPhysicsSystem()));
uiManager->addPanel(std::make_unique<SettingsPanel>(&engineState, ...));
```
**Root cause:** Panels receive raw EngineState pointer or direct state refs.

**Category C: Runtime loop (lines 302-346)**
```cpp
auto resources = engineState.resourceService().view();
auto rendering = engineState.renderingService().view();
auto sceneRuntime = engineState.sceneRuntimeService().view();
auto* animationSystem = engineState.animationRuntimeService().animation();
engineState.setPostProcessingSystem(...);
```
**Root cause:** Service accessors are fine, but `setPostProcessingSystem` and direct animation access bypass ports.

#### Gap 2: EngineState.hpp Concrete System Includes (16 headers)

Lines 13-28 include concrete system types directly:
```
AnimationSystem, CameraSystem, ColliderDebugRenderSystem, DeferredLightingSystem,
GridRenderSystem, IBLSystem, InputSystem, LODSystem, LightSystem,
ModelRenderSystem, ObjectSelectionSystem, PostProcessingSystem,
PhysicsSystem, JoltPhysicsSystem, ShadowSystem, SkyboxRenderSystem
```
Plus `ResourceManager.hpp` from ModelLib.

#### Gap 3: Missing Use Cases
- `LoadModelIntoSceneUseCase` - not implemented
- `ApplyMaterialChangesUseCase` - not implemented

#### Gap 4: New Architecture Tests Not Added
From original plan:
- `DeliveryMustNotAccessEngineStatePrivateMembers`
- `DomainMustNotDependOnInfrastructure`
- `ApplicationUseCasesMustOnlyDependOnPortsAndDomain`
- `RenderPassesMustNotIncludeDeliveryHeaders`
- `EngineStateMustNotIncludeVulkanTypesDirectly`

#### Gap 5: ADR Documentation Missing
4 ADR files planned, 0 written.

---

## Implementation Plan

### Step 1: Expand IRuntimeStatePort + Adapters
Add missing methods to cover all `engineState.XRef()` calls:

**IRuntimeStatePort additions:**
- `bool& showSkyboxRef()`
- `bool& showGridRef()`
- `SkyboxSettings& skySettingsRef()`
- `ShadowSettings& shadowSettingsRef()`
- `bool& solidGroundEnabledRef()`

**RuntimeStateAdapter:** Implement the new methods.

**ISceneRuntimeAccessPort additions:**
- `bool* showSkybox()`
- `bool* showGrid()`
- `bool* showDebugObjects()`

**SceneRuntimeAccessAdapter:** Implement the new methods.

### Step 2: Expand IPostProcessingAccessPort
Add method to get post-process descriptor set layout:
- `VkDescriptorSetLayout postProcessSetLayout() const`

**PostProcessingAccessAdapter:** Implement.

### Step 3: Refactor app.cpp - Eliminate Direct EngineState Access

**3a. Use RuntimeSettingsBindingService** instead of constructing RuntimeSettingsBindings directly.

**3b. Wire UI panels through ports** - pass port references / state from adapters instead of engineState.

**3c. Route post-processing recreation** through `IPostProcessingAccessPort`.

**3d. Route animation access** in render() through existing `AnimationAccessAdapter`.

### Step 4: Strip EngineState.hpp Concrete System Includes
Move system includes to a pimpl-style detail header or use forward declarations where possible.

### Step 5: Add Missing Use Cases
Implement `LoadModelIntoSceneUseCase` and `ApplyMaterialChangesUseCase`.

### Step 6: Add New Architecture Tests
Add tests from original plan to enforce boundaries programmatically.

### Step 7: Write ADRs
Document the 4 architecture decisions.

---

## Verification

After each step:
1. `xmake build` succeeds
2. All existing tests pass
3. No regression in direct engineState call count

Final verification:
1. `grep -c 'engineState\.' src/Editor/app.cpp` → only service accessor calls (`.XService().view()`)
2. `EngineState.hpp` has ≤3 concrete system includes (only what's needed for state services)
3. All architecture tests pass
4. ADR documentation exists

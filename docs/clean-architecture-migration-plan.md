# Clean Architecture / Hexagonal Architecture Migration Plan

## VulkanEngine

**Date:** 2025-06-02
**Status:** Draft
**Owner:** Engineering Team

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current State Analysis](#2-current-state-analysis)
3. [Target Architecture](#3-target-architecture)
4. [Migration Phases](#4-migration-phases)
5. [Architecture Tests](#5-architecture-tests)
6. [Risk Assessment](#6-risk-assessment)
7. [Appendix: File Inventory](#7-appendix-file-inventory)

---

## 1. Executive Summary

This plan documents the remaining work to achieve a complete Clean Architecture state in the VulkanEngine project. The migration has made significant progress: narrow state services, ports/use cases, and architecture dependency tests are in place. This plan focuses on closing the remaining gaps where delivery still calls engineState directly, infrastructure adapters still depend on concrete systems, and domain purity issues persist.

The target state is a four-layer architecture where:
- **Domain** has zero knowledge of infrastructure/delivery
- **Application** owns all orchestration and business logic
- **Infrastructure** implements ports used by application/domain
- **Delivery** only composes use cases and ports

---

## 2. Current State Analysis

### 2.1 Already Completed

#### Narrow State Services (Phase 1-2)

| Service | Location | Status |
|---------|----------|--------|
| `RenderingStateService` | `include/Engine/State/StateServices.hpp` | Complete |
| `SceneRuntimeService` | `include/Engine/State/StateServices.hpp` | Complete |
| `InputStateService` | `include/Engine/State/StateServices.hpp` | Complete |
| `ResourceStateService` | `include/Engine/State/StateServices.hpp` | Complete |
| `AnimationRuntimeService` | `include/Engine/State/StateServices.hpp` | Complete |
| `PhysicsRuntimeService` | `include/Engine/State/StateServices.hpp` | Complete |

#### State Views (Data Transfer Objects)

| View | Location | Status |
|------|----------|--------|
| `RenderingStateView` | `include/Engine/State/StateViews.hpp` | Complete |
| `SceneRuntimeStateView` | `include/Engine/State/StateViews.hpp` | Complete |
| `InputStateView` | `include/Engine/State/StateViews.hpp` | Complete |
| `ResourceStateView` | `include/Engine/State/StateViews.hpp` | Complete |
| `SystemServicesView` | `include/Engine/State/StateViews.hpp` | Complete (private) |

#### Ports (Application Layer Interfaces)

| Port | Location | Methods |
|------|----------|---------|
| `IScenePersistencePort` | `include/Engine/Application/Ports/` | `saveScene`, `loadScene` |
| `IPhysicsRuntimePort` | `include/Engine/Application/Ports/` | `clearSceneBodies`, `setGroundEnabled` |
| `IEnvironmentLightingPort` | `include/Engine/Application/Ports/` | `syncEnvironmentLighting` |
| `ISceneSelectionMaintenancePort` | `include/Engine/Application/Ports/` | `processSelectionMaintenance` |

#### Use Cases (Application Layer)

| Use Case | Location | Depends On |
|----------|----------|------------|
| `LoadSceneUseCase` | `include/Engine/Application/UseCases/` | `IScenePersistencePort`, `IPhysicsRuntimePort`, `Scene`, `SceneRuntimeState` |
| `SaveSceneUseCase` | `include/Engine/Application/UseCases/` | `IScenePersistencePort` |
| `ReconcileSceneLoadUseCase` | `include/Engine/Application/UseCases/` | `Scene`, `SceneRuntimeState` |
| `SyncEnvironmentLightingUseCase` | `include/Engine/Application/UseCases/` | `IEnvironmentLightingPort` |
| `ProcessSceneSelectionMaintenanceUseCase` | `include/Engine/Application/UseCases/` | `ISceneSelectionMaintenancePort`, `SceneRuntimeState` |

#### Infrastructure Adapters (Delivery -> Infrastructure implementations)

| Adapter | Implements | Depends On |
|---------|-----------|------------|
| `ScenePersistenceAdapter` | `IScenePersistencePort` | `SceneSerializer` |
| `PhysicsRuntimeAdapter` | `IPhysicsRuntimePort` | `JoltPhysicsSystem*` |
| `EnvironmentLightingAdapter` | `IEnvironmentLightingPort` | `Device`, `EngineState` |

#### Architecture Dependency Tests

14 architecture tests in `tests/Engine/ArchitectureDependencyRulesTests.cpp`:
- Engine headers must not include Editor headers
- Engine sources must not include Editor headers (with allowlist)
- Application headers must not depend on Infrastructure or Delivery
- Application sources must not depend on Editor/delivery layer
- Domain headers must not depend on Application or Editor
- Domain sources must not depend on Application or Editor
- Infrastructure adapters must only depend on ports
- Infrastructure adapter sources must only depend on ports and runtime
- Runtime systems must not depend on Application layer
- Delivery app must not perform post-load camera reconciliation directly
- Delivery app must not perform environment lighting/descriptor orchestration directly
- Grouped state accessors should be replaced by narrow state services
- Editor UI should use narrow services instead of legacy getters
- Render passes should use state services instead of legacy getters
- Migrated panels should use state services for runtime queries
- Scene panel should use scene runtime service for scene access

#### Engine Lifecycle Contract Tests

8 tests in `tests/Engine/EngineLifecycleContractTests.cpp` verifying:
- Explicit RenderContext parameter in EngineState::initialize
- Post-processing system recreation on swapchain recreation
- Scene loading wired through Application use cases
- EngineState no longer owns Editor UI objects
- EngineState provides grouped system services accessor
- Hot paths use EngineState system accessors not direct members

#### RuntimeSettingsBindingService

`include/Engine/State/RuntimeSettingsBindingService.hpp` - An application-level service that owns the logic of building and applying runtime settings bindings to a SceneSerializer. Delivery no longer constructs RuntimeSettingsBindings directly.

### 2.2 Remaining Issues

#### Issue 1: Direct EngineState Calls from Delivery (app.cpp)

The `App` class in `src/Editor/app.cpp` directly accesses EngineState members and services in multiple places:

- **init()**: Calls `engineState.sceneRuntimeService().view().scene` to get the scene pointer for `SceneSerializer` construction
- **init()**: Calls `engineState.showSkyboxRef()`, `engineState.showGridRef()`, `engineState.showDebugObjectsRef()`, `engineState.physicsSimulationRunningRef()`, `engineState.skySettingsRef()`, `engineState.postProcessPushRef()` - these are direct state accessor calls
- **init()**: Calls `engineState.physicsRuntimeService().joltPhysics()` to get JoltPhysicsSystem for `PhysicsRuntimeAdapter` construction
- **setupUI()**: Passes `&engineState.physicsSimulationRunningRef()`, `&engineState.showColliderWireframesRef()`, `&engineState.solidGroundEnabledRef()` directly to UI panels
- **setupUI()**: Passes `engineState_->physicsRuntimeService().joltPhysics()` to InspectorPanel
- **setupRenderGraph()**: Passes `&engineState` to all render passes (UpdatePass, ComputePass, ShadowPass, DepthPrepass, OffscreenPass, CompositionPass)
- **render()**: Calls `engineState.animationRuntimeService().animation()` to get AnimationSystem*
- **render()**: Calls `engineState.renderingService().view()`, `engineState.sceneRuntimeService().view()` repeatedly
- **render()**: Calls `engineState.setPostProcessingSystem()` on swapchain recreation
- **sceneRuntimeState()**: Builds `SceneRuntimeState` by calling multiple EngineState accessors

**Impact**: Delivery is tightly coupled to EngineState's internal structure. Any change to EngineState's internals requires delivery changes.

#### Issue 2: Infrastructure Adapter Dependencies

**`EnvironmentLightingAdapter`** (`src/Editor/Infrastructure/EnvironmentLightingAdapter.cpp`):
- Depends on `EngineState` directly (not through ports)
- Calls `engineState_.renderingService().view()` to access IBLSystem
- Calls `engineState_.sceneRuntimeService().view()` to access skybox
- Calls `engineState_.skyboxRef()` to set skybox
- Calls `engineState_.deferredIblDescriptorSetsRef()` to update descriptors
- Calls `engineState_.deferredIblSetLayoutRef()` and `engineState_.deferredIblPoolRef()` for descriptor writing
- Calls `engineState_.getPostProcessDescriptorSet()` indirectly through DescriptorWriter
- Directly manipulates Vulkan descriptor sets

**`PhysicsRuntimeAdapter`** (`src/Editor/Infrastructure/PhysicsRuntimeAdapter.cpp`):
- Depends on `JoltPhysicsSystem*` directly (not through a port)
- This is actually acceptable - it's a narrow dependency from Infrastructure to a specific domain entity, but it bypasses the port abstraction

**`ScenePersistenceAdapter`** (`src/Editor/Infrastructure/ScenePersistenceAdapter.cpp`):
- Clean - only depends on `SceneSerializer` which is a domain/infrastructure type

#### Issue 3: Domain Purity Issues

**EngineState.hpp** (`include/Engine/EngineState.hpp`):
- Directly includes Vulkan types (`VkDescriptorSet`, `VkDescriptorSetLayout`)
- Directly includes `Device`, `Renderer`, `ResourceManager` dependencies
- Contains `SystemRegistry` which is infrastructure-adjacent
- Has `friend` declarations for state services
- Private members include concrete system types, not abstract interfaces

**Render Passes** (`include/Engine/Graphics/Passes/`):
- All passes (`UpdatePass`, `ShadowPass`, `OffscreenPass`, `CompositionPass`, `DepthPrepass`, `ComputePass`, `UpdatePass`) take `EngineState*` directly
- They use `engineState_->renderingService().view()` pattern but also access `engineState_` directly for descriptor sets, pools, and layouts
- `CompositionPass` includes `Editor/ui/UIManager.hpp` (direct delivery dependency in infrastructure)
- `ShadowPass` includes `Editor/RenderContext.hpp` (direct delivery dependency in infrastructure)
- `OffscreenPass` includes `Editor/RenderContext.hpp` (direct delivery dependency in infrastructure)
- These are the files in the architecture test allowlist

**`StateServices.cpp`** (`src/Engine/State/StateServices.cpp`):
- Includes concrete system headers (`AnimationSystem.hpp`, `PhysicsSystem.hpp`, `JoltPhysicsSystem.hpp`)
- This is acceptable as it's the bridge between state services and concrete implementations

#### Issue 4: Missing Use Cases for Editor Commands

Several editor actions are not wrapped in use cases:

| Editor Action | Current State | Target Use Case |
|---------------|---------------|-----------------|
| Add entity to scene | Direct in ScenePanel | `AddEntityUseCase` |
| Delete entity | Direct in ScenePanel | `DeleteEntityUseCase` |
| Set active camera | Direct in ScenePanel | `SetActiveCameraUseCase` |
| Load model into scene | Direct in ScenePanel | `LoadModelIntoSceneUseCase` |
| Toggle physics simulation | Direct via reference | `TogglePhysicsSimulationUseCase` |
| Change skybox settings | Direct via reference | `ChangeSkyboxSettingsUseCase` |
| Change shadow settings | Direct via reference | `ChangeShadowSettingsUseCase` |
| Apply material changes | Direct in InspectorPanel | `ApplyMaterialChangesUseCase` |
| Transform manipulation | Direct in TransformPanel | `ApplyTransformUseCase` |

#### Issue 5: EngineState as God Object

`EngineState` owns:
- 15+ concrete system types
- Descriptor pools and layouts
- Scene and skybox
- Input devices
- All rendering state flags
- All scene settings

This makes it the primary bottleneck for Clean Architecture compliance. It's the single point where all layers converge.

#### Issue 6: RenderContext Dependency

`RenderContext` is constructed in `App::init()` and passed to `EngineState::initialize()`. It's also included in render passes and `CompositionPass` includes `Editor/ui/UIManager.hpp`. The `RenderContext` type bridges delivery infrastructure to engine internals.

### 2.3 Architecture Test Allowlist Status

The architecture tests have a transitional allowlist for Engine->Editor includes in:
- `src/Engine/EngineState.cpp`
- `src/Engine/Graphics/Passes/CompositionPass.cpp`
- `src/Engine/Graphics/Passes/OffscreenPass.cpp`
- `src/Engine/Graphics/Passes/ShadowPass.cpp`

These should be reduced over time as the migration progresses.

---

## 3. Target Architecture

### 3.1 Layer Structure

```
+-------------------------------------------------------------------+
|                        DELIVERY (Editor)                          |
|  include/Editor/  src/Editor/                                     |
|                                                                   |
|  - App (composition root)                                         |
|  - UI Panels (ScenePanel, InspectorPanel, etc.)                   |
|  - Infrastructure Adapters (ScenePersistenceAdapter, etc.)        |
|  - RenderContext (delivery-specific render abstraction)           |
|  - main.cpp                                                       |
|                                                                   |
|  Responsibilities:                                                |
|  - Compose use cases and ports                                    |
|  - Build concrete adapter instances                               |
|  - Handle platform-specific concerns (GLFW, Vulkan initialization) |
|  - Present state to user through UI                               |
+-------------------------------------------------------------------+
           |      depends on      |      implements      |
           v                      v                      |
+-------------------------------------------------------------------+
|                      APPLICATION (Use Cases)                      |
|  include/Engine/Application/                                      |
|  src/Engine/Application/                                          |
|                                                                   |
|  - Ports (IScenePersistencePort, IPhysicsRuntimePort, ...)        |
|  - Use Cases (LoadSceneUseCase, SaveSceneUseCase, ...)            |
|  - SceneRuntimeState (shared state DTO)                           |
|  - RuntimeSettingsBindingService                                  |
|                                                                   |
|  Responsibilities:                                                |
|  - Define interfaces (ports) for infrastructure                   |
|  - Orchestrate use case execution                                 |
|  - Contain business logic for scene operations                    |
|  - Define domain events and contracts                             |
+-------------------------------------------------------------------+
           |      depends on      |      implements      |
           v                      v                      |
+-------------------------------------------------------------------+
|                          DOMAIN                                   |
|  include/Engine/Scene/  include/Engine/Core/                      |
|  include/Engine/Systems/                                          |
|  src/Engine/Scene/  src/Engine/Core/                              |
|                                                                   |
|  - Components (TransformComponent, CameraComponent, etc.)         |
|  - Scene (entt registry wrapper)                                  |
|  - Camera (view/projection math)                                  |
|  - Systems (AnimationSystem, PhysicsSystem, etc.)                 |
|  - Core utilities (Logger, Keyboard, Mouse, Window)               |
|                                                                   |
|  Responsibilities:                                                |
|  - Pure domain entities and components                            |
|  - Domain invariants and business rules                           |
|  - System logic (animation, physics, rendering)                   |
|  - Zero knowledge of infrastructure or delivery                   |
+-------------------------------------------------------------------+
           |      depends on      |      implements      |
           v                      v                      |
+-------------------------------------------------------------------+
                       INFRASTRUCTURE                                |
|  include/Engine/Graphics/  include/EngineSceneIO/                 |
|  include/ModelLib/                                                |
|  src/Engine/Graphics/  src/EngineSceneIO/  src/ModelLib/          |
|                                                                   |
|  - Vulkan abstractions (Device, Renderer, SwapChain, etc.)        |
|  - Render passes (UpdatePass, ShadowPass, OffscreenPass, etc.)    |
|  - Model importers (GLTFImporter, OBJImporter)                    |
|  - Resource management (ResourceManager, TextureManager, etc.)    |
|  - Scene serialization (SceneSerializer)                          |
|  - IBL baking tools                                               |
|                                                                   |
|  Responsibilities:                                                |
|  - Implement ports defined by application                         |
|  - Provide concrete infrastructure implementations                |
|  - Handle Vulkan/GPU resource management                          |
|  - File I/O and model loading                                     |
+-------------------------------------------------------------------+
```

### 3.2 Dependency Rules

```
Domain     -> Domain only (no external dependencies)
Application -> Domain (use case inputs/outputs)
Application -> Ports (self-referential interface definitions)
Infrastructure -> Domain (implements ports, uses domain types)
Infrastructure -> Infrastructure (cross-infrastructure deps)
Delivery -> Application (compose use cases, instantiate adapters)
Delivery -> Infrastructure (use concrete implementations)
Delivery -> Delivery (UI panels, render context)
```

### 3.3 Key Architectural Decisions

#### Decision 1: EngineState as Application-Layer Coordinator

EngineState should not be a domain type or infrastructure type. It is an **application-layer coordinator** that:
- Owns the lifecycle of systems
- Provides state views (not direct access)
- Coordinates system initialization
- Should not expose direct member access

#### Decision 2: Render Passes as Infrastructure, Not Delivery

Render passes currently include Editor headers (CompositionPass includes UIManager, ShadowPass/OffscreenPass include RenderContext). They should:
- Accept only domain/application types through their constructors
- Use interfaces (ports) where possible
- Not include any delivery-layer headers

#### Decision 3: EnvironmentLightingAdapter Port Expansion

The current `IEnvironmentLightingPort` is too narrow for what EnvironmentLightingAdapter does. The port should be expanded or split to capture all the operations the adapter performs.

#### Decision 4: PhysicsRuntimeAdapter Dependency

PhysicsRuntimeAdapter depends on `JoltPhysicsSystem*` directly. This is acceptable as a narrow dependency but should be documented. Consider whether a `IJoltPhysicsPort` would be more architecturally sound.

#### Decision 5: SceneRuntimeState as Shared DTO

`SceneRuntimeState` is a shared DTO that flows from Delivery into Application use cases. It should be treated as an application-layer type, not a domain type. It contains references to mutable state that use cases modify.

---

## 4. Migration Phases

### Phase 0: Foundation (No Changes Required)

The narrow state services, ports, use cases, and architecture tests are already in place. This phase is complete.

**Exit Criteria:** All existing architecture tests pass.

### Phase 1: Eliminate Direct EngineState Member Access from Delivery

#### 1.1: Create `ISceneRuntimeAccessPort`

**Goal:** Provide a port that allows delivery to query and modify scene runtime state without knowing EngineState's internals.

**Files to create/modify:**
- `include/Engine/Application/Ports/ISceneRuntimeAccessPort.hpp` (new)
- `include/Editor/Infrastructure/SceneRuntimeAccessAdapter.hpp` (new)
- `src/Editor/Infrastructure/SceneRuntimeAccessAdapter.cpp` (new)

**Port interface:**
```cpp
class ISceneRuntimeAccessPort {
 public:
  virtual ~ISceneRuntimeAccessPort() = default;
  virtual Scene* scene() = 0;
  virtual entt::entity& selectedEntity() = 0;
  virtual entt::entity& cameraEntity() = 0;
  virtual bool& showSkybox() = 0;
  virtual bool& showGrid() = 0;
  virtual bool& showDebugObjects() = 0;
  virtual bool& showColliderWireframes() = 0;
  virtual bool& physicsSimulationRunning() = 0;
  virtual bool& solidGroundEnabled() = 0;
  virtual SkyboxSettings& skySettings() = 0;
  virtual ShadowSettings& shadowSettings() = 0;
  virtual PostProcessPushConstants& postProcessPush() = 0;
};
```

**Adapter implementation:** Wraps EngineState service calls behind the port interface.

**App changes:**
- Construct `SceneRuntimeAccessAdapter` from EngineState
- Pass it to use cases and panels that need runtime state access
- Remove direct calls to `engineState.showSkyboxRef()`, `engineState.showGridRef()`, etc.

#### 1.2: Create `IAnimationAccessPort`

**Goal:** Allow delivery to access the animation system without depending on EngineState.

**Files to create/modify:**
- `include/Engine/Application/Ports/IAnimationAccessPort.hpp` (new)
- `include/Editor/Infrastructure/AnimationAccessAdapter.hpp` (new)
- `src/Editor/Infrastructure/AnimationAccessAdapter.cpp` (new)

**Port interface:**
```cpp
class IAnimationAccessPort {
 public:
  virtual ~IAnimationAccessPort() = default;
  virtual MorphTargetManager* getMorphManager() = 0;
};
```

**App changes:**
- Construct `AnimationAccessAdapter` from `engineState.animationRuntimeService().animation()`
- Pass it to the render pipeline or FrameInfo builder

#### 1.3: Create `IPostProcessingAccessPort`

**Goal:** Allow delivery to recreate the post-processing system on swapchain events without knowing EngineState's internals.

**Files to create/modify:**
- `include/Engine/Application/Ports/IPostProcessingAccessPort.hpp` (new)
- `include/Editor/Infrastructure/PostProcessingAccessAdapter.hpp` (new)
- `src/Editor/Infrastructure/PostProcessingAccessAdapter.cpp` (new)

**Port interface:**
```cpp
class IPostProcessingAccessPort {
 public:
  virtual ~IPostProcessingAccessPort() = default;
  virtual void recreatePostProcessingSystem(Device& device,
      VkRenderPass renderPass,
      std::vector<VkDescriptorSetLayout> setLayouts) = 0;
};
```

#### 1.4: Update App::init() and App::render()

**Goal:** Remove all direct EngineState member access from App.

**Changes in `App::init()`:**
- Replace `engineState.showSkyboxRef()` etc. with port access
- Replace `engineState.physicsRuntimeService().joltPhysics()` with a port
- Pass port references instead of EngineState* to use cases where possible

**Changes in `App::render()`:**
- Replace `engineState.animationRuntimeService().animation()` with port access
- Replace `engineState.renderingService().view()` with port access
- Replace `engineState.setPostProcessingSystem()` with port call

**Exit Criteria:**
- `App::init()` has zero calls to `engineState.XRef()` accessor methods
- `App::render()` has zero calls to `engineState.XRef()` accessor methods
- `App::render()` has zero calls to `engineState.animationRuntimeService()`
- `App::render()` has zero calls to `engineState.setPostProcessingSystem()`
- Architecture test `DeliveryAppMustNotPerformEnvironmentLightingOrDescriptorOrchestration` still passes
- All existing tests pass

### Phase 2: Eliminate Direct EngineState from Render Passes

#### 2.1: Redefine Render Pass Interfaces

**Goal:** Render passes should accept domain/application types, not EngineState*.

**Changes to pass constructors:**

| Pass | Current Constructor | Target Constructor |
|------|-------------------|-------------------|
| UpdatePass | `UpdatePass(EngineState*, Renderer&)` | `UpdatePass(SystemServicesView, Renderer&)` |
| ShadowPass | `ShadowPass(EngineState*)` | `ShadowPass(RenderingStateView, SceneRuntimeStateView)` |
| OffscreenPass | `OffscreenPass(Renderer&, EngineState*, Device&, int&)` | `OffscreenPass(Renderer&, RenderingStateView, ResourceStateView, int&)` |
| CompositionPass | `CompositionPass(Renderer&, EngineState*, UIManager*, Camera&, Window&)` | `CompositionPass(Renderer&, RenderingStateView, UIManager*, Camera&, Window&)` |
| ComputePass | `ComputePass(EngineState*)` | `ComputePass(RenderingStateView)` |
| DepthPrepass | `DepthPrepass(EngineState*, Renderer&)` | `DepthPrepass(RenderingStateView, Renderer&)` |

**Files to modify:**
- `include/Engine/Graphics/Passes/UpdatePass.hpp`
- `include/Engine/Graphics/Passes/ShadowPass.hpp`
- `include/Engine/Graphics/Passes/OffscreenPass.hpp`
- `include/Engine/Graphics/Passes/CompositionPass.hpp`
- `include/Engine/Graphics/Passes/ComputePass.hpp`
- `include/Engine/Graphics/Passes/DepthPrepass.hpp`
- `src/Engine/Graphics/Passes/UpdatePass.cpp`
- `src/Engine/Graphics/Passes/ShadowPass.cpp`
- `src/Engine/Graphics/Passes/OffscreenPass.cpp`
- `src/Engine/Graphics/Passes/CompositionPass.cpp`
- `src/Engine/Graphics/Passes/ComputePass.cpp`
- `src/Engine/Graphics/Passes/DepthPrepass.cpp`

#### 2.2: Create `ICompositionPort` for UIManager

**Goal:** CompositionPass should not include UIManager directly.

**Files to create/modify:**
- `include/Engine/Application/Ports/ICompositionPort.hpp` (new)
- `include/Editor/Infrastructure/CompositionPortAdapter.hpp` (new)
- `src/Editor/Infrastructure/CompositionPortAdapter.cpp` (new)

**Port interface:**
```cpp
class ICompositionPort {
 public:
  virtual ~ICompositionPort() = default;
  virtual void renderUI(FrameInfo&, VkCommandBuffer) = 0;
};
```

**Adapter implementation:** Wraps `uiManager_->render(frameInfo, commandBuffer, true)`.

**CompositionPass changes:**
- Replace `UIManager* uiManager_` with `ICompositionPort* compositionPort_`
- Remove `#include "Editor/ui/UIManager.hpp"` from CompositionPass.hpp
- Remove `#include "Editor/ui/UIManager.hpp"` from CompositionPass.cpp

#### 2.3: Create `IRenderContextPort` for RenderContext

**Goal:** ShadowPass and OffscreenPass should not include RenderContext directly.

**Files to create/modify:**
- `include/Engine/Application/Ports/IRenderContextPort.hpp` (new)
- `include/Editor/Infrastructure/RenderContextPortAdapter.hpp` (new)
- `src/Editor/Infrastructure/RenderContextPortAdapter.cpp` (new)

**Port interface:**
```cpp
class IRenderContextPort {
 public:
  virtual ~IRenderContextPort() = default;
  virtual LightCounts updateLightBuffers(int frameIndex, Scene& scene) = 0;
  virtual VkDescriptorSet getGlobalDescriptorSet(int frameIndex) = 0;
  virtual void updateUBO(int frameIndex, GlobalUbo const& ubo, GlobalUboCold const& uboCold) = 0;
};
```

**App changes in setupRenderGraph():**
- Pass `IRenderContextPort*` to passes that need it
- Construct `RenderContextPortAdapter` from RenderContext*

#### 2.4: Remove CompositionPass from Allowlist

**Goal:** Eliminate CompositionPass from the architecture test allowlist.

**Changes:**
- Remove `src/Engine/Graphics/Passes/CompositionPass.cpp` from `allowedFiles` in `ArchitectureDependencyRules.EngineSourcesMustNotIncludeEditorHeaders`

**Exit Criteria:**
- All render pass headers accept only domain/application types
- No render pass includes any `Editor/` header
- CompositionPass.cpp no longer includes UIManager.hpp
- ShadowPass.cpp and OffscreenPass.cpp no longer include RenderContext.hpp
- Architecture test `EngineSourcesMustNotIncludeEditorHeaders` passes with empty allowlist
- All existing tests pass

### Phase 3: Expand Infrastructure Adapter Port Coverage

#### 3.1: Expand `IEnvironmentLightingPort`

**Goal:** The current port is too narrow. EnvironmentLightingAdapter does descriptor manipulation that should be exposed through the port.

**Files to modify:**
- `include/Engine/Application/Ports/IEnvironmentLightingPort.hpp`

**New port interface:**
```cpp
class IEnvironmentLightingPort {
 public:
  virtual ~IEnvironmentLightingPort() = default;
  virtual void syncEnvironmentLighting(bool showSkyboxEnabled) = 0;
  virtual void setSkybox(std::unique_ptr<Skybox> skybox) = 0;
  virtual void resetToFallback() = 0;
  virtual void updateDeferredIBLDescriptors(int frameIndex,
      VkDescriptorSet descriptorSet,
      VkDescriptorPool pool,
      VkDescriptorSetLayout layout) = 0;
};
```

**Changes to EnvironmentLightingAdapter:**
- Remove direct EngineState member access
- Use the expanded port methods instead
- Keep Device as constructor dependency (it's needed for Vulkan operations)

#### 3.2: Create `ISkyboxAccessPort`

**Goal:** Allow EnvironmentLightingAdapter to set/get skybox through a port instead of EngineState.

**Files to create/modify:**
- `include/Engine/Application/Ports/ISkyboxAccessPort.hpp` (new)
- `include/Editor/Infrastructure/SkyboxAccessAdapter.hpp` (new)
- `src/Editor/Infrastructure/SkyboxAccessAdapter.cpp` (new)

**Port interface:**
```cpp
class ISkyboxAccessPort {
 public:
  virtual ~ISkyboxAccessPort() = default;
  virtual std::unique_ptr<Skybox>& skyboxRef() = 0;
  virtual Skybox* skybox() const = 0;
};
```

#### 3.3: Create `IDescriptorAccessPort`

**Goal:** Allow EnvironmentLightingAdapter to manipulate descriptor sets through a port.

**Files to create/modify:**
- `include/Engine/Application/Ports/IDescriptorAccessPort.hpp` (new)
- `include/Editor/Infrastructure/DescriptorAccessAdapter.hpp` (new)
- `src/Editor/Infrastructure/DescriptorAccessAdapter.cpp` (new)

**Port interface:**
```cpp
class IDescriptorAccessPort {
 public:
  virtual ~IDescriptorAccessPort() = default;
  virtual VkDescriptorSet& deferredShadowDescriptorSetRef(int frameIndex) = 0;
  virtual VkDescriptorSet& deferredIblDescriptorSetRef(int frameIndex) = 0;
  virtual VkDescriptorSet& postProcessDescriptorSetRef(int frameIndex) = 0;
  virtual VkDescriptorSet& gbufferDescriptorSetRef(int frameIndex) = 0;
  virtual DescriptorPool& deferredIblPoolRef() = 0;
  virtual DescriptorSetLayout& deferredIblSetLayoutRef() = 0;
  virtual DescriptorPool& gbufferPoolRef() = 0;
  virtual DescriptorSetLayout& gbufferSetLayoutRef() = 0;
  virtual DescriptorSetLayout& postProcessSetLayoutRef() = 0;
  virtual VkDescriptorSet getGbufferDescriptorSet(int frameIndex) const = 0;
  virtual VkDescriptorSet getDeferredShadowDescriptorSet(int frameIndex) const = 0;
  virtual VkDescriptorSet getDeferredIblDescriptorSet(int frameIndex) const = 0;
  virtual std::vector<VkDescriptorSet>& deferredIblDescriptorSetsRef() = 0;
};
```

**Note:** This is a large port. Consider whether it should be split into `IDescriptorPoolPort` and `IDescriptorSetPort` or whether the adapter should have a more targeted interface.

#### 3.4: Create `ISceneAccessPort`

**Goal:** Allow EnvironmentLightingAdapter to access scene state through a port.

**Files to create/modify:**
- `include/Engine/Application/Ports/ISceneAccessPort.hpp` (new)
- `include/Editor/Infrastructure/SceneAccessAdapter.hpp` (new)
- `src/Editor/Infrastructure/SceneAccessAdapter.cpp` (new)

**Port interface:**
```cpp
class ISceneAccessPort {
 public:
  virtual ~ISceneAccessPort() = default;
  virtual Scene* scene() = 0;
  virtual Skybox* skybox() const = 0;
  virtual SkyboxSettings* skySettings() = 0;
  virtual ShadowSettings* shadowSettings() = 0;
  virtual entt::entity* selectedEntity() = 0;
  virtual entt::entity* cameraEntity() = 0;
};
```

#### 3.5: Refactor EnvironmentLightingAdapter

**Goal:** Remove all direct EngineState member access from EnvironmentLightingAdapter.

**Changes to `EnvironmentLightingAdapter`:**
- Constructor takes `Device&` and `IEnvironmentLightingPort&` (or the expanded port)
- Remove `EngineState& engineState_` member
- Replace all `engineState_.X()` calls with port calls
- Remove `#include "Engine/EngineState.hpp"` from the adapter

#### 3.6: Update PhysicsRuntimeAdapter Dependency

**Goal:** Document or address the `JoltPhysicsSystem*` direct dependency.

**Options:**
1. **Accept as-is:** Document that this is a narrow, intentional dependency from Infrastructure to a specific domain entity. The port abstraction is preserved at the application layer.
2. **Create `IJoltPhysicsPort`:** Extract physics operations into a port and have the adapter depend on it.

**Recommendation:** Option 1 for now. The PhysicsRuntimeAdapter's dependency on JoltPhysicsSystem is narrow and intentional. It implements `IPhysicsRuntimePort` which is the correct application-layer abstraction. The concrete dependency is acceptable.

#### 3.7: Update Architecture Tests

**Files to modify:**
- `tests/Engine/ArchitectureDependencyRulesTests.cpp`

**Changes:**
- Remove `src/Editor/Infrastructure/EnvironmentLightingAdapter.cpp` from the allowlist
- Add tests for new infrastructure adapter ports
- Update the `InfrastructureAdaptersMustOnlyDependOnPorts` test to include the new port checks
- Update the `InfrastructureAdaptersSourcesMustOnlyDependOnPortsAndRuntime` test

**Exit Criteria:**
- EnvironmentLightingAdapter has zero direct EngineState member access
- EnvironmentLightingAdapter has zero includes of `Engine/EngineState.hpp`
- All infrastructure adapters only depend on ports and runtime types
- Architecture tests for infrastructure adapters pass
- All existing tests pass

### Phase 4: Add Missing Use Cases

#### 4.1: `AddEntityUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/AddEntityUseCase.hpp`
- `src/Engine/Application/UseCases/AddEntityUseCase.cpp`

**Port dependencies:** `ISceneAccessPort`

**Purpose:** Add a new entity to the scene with optional components.

#### 4.2: `DeleteEntityUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/DeleteEntityUseCase.hpp`
- `src/Engine/Application/UseCases/DeleteEntityUseCase.cpp`

**Port dependencies:** `ISceneAccessPort`

**Purpose:** Delete an entity from the scene.

#### 4.3: `SetActiveCameraUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/SetActiveCameraUseCase.hpp`
- `src/Engine/Application/UseCases/SetActiveCameraUseCase.cpp`

**Port dependencies:** `ISceneRuntimeAccessPort`

**Purpose:** Set the active camera entity.

#### 4.4: `LoadModelIntoSceneUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/LoadModelIntoSceneUseCase.hpp`
- `src/Engine/Application/UseCases/LoadModelIntoSceneUseCase.cpp`

**Port dependencies:** `IResourceManagerPort`, `ISceneAccessPort`

**Purpose:** Load a model into the scene, create entity with ModelComponent, etc.

#### 4.5: `TogglePhysicsSimulationUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/TogglePhysicsSimulationUseCase.hpp`
- `src/Engine/Application/UseCases/TogglePhysicsSimulationUseCase.cpp`

**Port dependencies:** `ISceneRuntimeAccessPort`, `IPhysicsRuntimePort`

**Purpose:** Toggle physics simulation running state.

#### 4.6: `ChangeSkyboxSettingsUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/ChangeSkyboxSettingsUseCase.hpp`
- `src/Engine/Application/UseCases/ChangeSkyboxSettingsUseCase.cpp`

**Port dependencies:** `ISceneRuntimeAccessPort`, `IEnvironmentLightingPort`

**Purpose:** Change skybox settings and trigger IBL update.

#### 4.7: `ChangeShadowSettingsUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/ChangeShadowSettingsUseCase.hpp`
- `src/Engine/Application/UseCases/ChangeShadowSettingsUseCase.cpp`

**Port dependencies:** `ISceneRuntimeAccessPort`

**Purpose:** Change shadow settings.

#### 4.8: `ApplyTransformUseCase`

**Files to create:**
- `include/Engine/Application/UseCases/ApplyTransformUseCase.hpp`
- `src/Engine/Application/UseCases/ApplyTransformUseCase.cpp`

**Port dependencies:** `ISceneAccessPort`

**Purpose:** Apply transform changes to the selected entity.

#### 4.9: Update Architecture Tests for Use Cases

**Files to modify:**
- `tests/Engine/ArchitectureDependencyRulesTests.cpp`

**New tests:**
- `ApplicationUseCasesMustNotDependOnDeliveryLayer` - All use case files must not include Editor headers
- `ApplicationUseCasesMustNotDependOnEngineState` - Use cases must not include EngineState.hpp
- `ApplicationUseCasesMustOnlyDependOnPortsAndDomain` - Use case headers must only depend on ports and domain types

**Exit Criteria:**
- All editor commands go through use cases
- No use case includes EngineState.hpp
- No use case includes any Editor header
- All use case architecture tests pass
- All existing tests pass

### Phase 5: EngineState Refactoring

#### 5.1: Remove Vulkan Types from EngineState.hpp

**Goal:** EngineState should not directly include Vulkan types.

**Changes:**
- Forward-declare `VkDescriptorSet`, `VkDescriptorSetLayout` in EngineState.hpp
- Move Vulkan-specific methods to a separate `EngineStateVulkan.hpp` header
- Update EngineState.cpp to include the Vulkan header

**Files to create/modify:**
- `include/Engine/EngineState.hpp` (modify)
- `include/Engine/EngineStateVulkan.hpp` (new)
- `src/Engine/EngineState.cpp` (modify)

#### 5.2: Create `IEngineStatePort` (Optional, Long-term)

**Goal:** Provide a port that allows delivery to interact with EngineState without knowing its internals.

**This is a significant refactor.** For now, Phase 1-3 should provide sufficient decoupling.

**Consider this for a future Phase 6 if needed.**

#### 5.3: Make SystemRegistry Accessor-Only

**Goal:** Systems should not be directly accessible from EngineState members.

**Changes:**
- Remove direct system member access from EngineState.hpp
- Provide accessor methods for each system through state services
- Update all callers to use state services

**Note:** This is largely already done. The remaining work is ensuring no infrastructure code directly accesses system members.

#### 5.4: Remove CompositionPass from Allowlist

**Already in Phase 2.**

**Exit Criteria:**
- EngineState.hpp has zero Vulkan type includes
- All system access goes through state services
- EngineStateVulkan.hpp exists and is only included by EngineState.cpp
- All existing tests pass

### Phase 6: Final Cleanup

#### 6.1: Remove All Architecture Test Allowlists

**Goal:** Eliminate all transitional allowlists from architecture tests.

**Files to modify:**
- `tests/Engine/ArchitectureDependencyRulesTests.cpp`
- `tests/Engine/EngineLifecycleContractTests.cpp`

**Changes:**
- Remove all `allowedFiles` sets from architecture tests
- Ensure all files pass without allowlists
- Remove any remaining allowlist comments

#### 6.2: Add Comprehensive Architecture Test Suite

**Files to create:**
- `tests/Engine/Architecture/DependencyGraphTests.cpp` (new)
- `tests/Engine/Architecture/UseCaseTests.cpp` (new)
- `tests/Engine/Architecture/PortTests.cpp` (new)

**New tests:**
1. **Dependency Graph Tests:**
   - `DomainMustNotDependOnApplication` - Domain files must not include Application headers
   - `DomainMustNotDependOnInfrastructure` - Domain files must not include Graphics/Systems/ModelLib headers
   - `ApplicationMustNotDependOnInfrastructure` - Application files must not include Graphics/Systems/ModelLib headers
   - `ApplicationMustNotDependOnDelivery` - Application files must not include Editor headers
   - `InfrastructureMustNotDependOnDelivery` - Infrastructure files must not include Editor/ui headers
   - `DeliveryMustNotDependOnEngineStateInternals` - Delivery must not access EngineState private members

2. **Use Case Tests:**
   - `AllEditorCommandsHaveUseCases` - Verify all significant editor actions have use case coverage
   - `UseCasesDoNotBypassPorts` - Verify use cases only interact through ports

3. **Port Tests:**
   - `AllPortsHaveImplementations` - Verify all ports have exactly one implementation
   - `PortsAreMinimal` - Verify ports expose only necessary operations

#### 6.3: Update xmake.lua for New Architecture

**Files to modify:**
- `xmake.lua`

**Changes:**
- Add architecture test target
- Ensure new use case files are compiled
- Update test asset copying if needed

#### 6.4: Document Architecture Decisions

**Files to create:**
- `docs/architecture-decisions/001-clean-architecture-migration.md` (ADR for this migration)
- `docs/architecture-decisions/002-engine-state-as-coordinator.md` (ADR for EngineState role)
- `docs/architecture-decisions/003-state-services-pattern.md` (ADR for state services pattern)
- `docs/architecture-decisions/004-render-pass-interface.md` (ADR for render pass interface)

**Exit Criteria:**
- Zero architecture test allowlists
- Comprehensive architecture test suite in place
- Architecture decision records documented
- All tests pass
- Build succeeds

---

## 5. Architecture Tests

### 5.1 Existing Tests (Already Implemented)

See `tests/Engine/ArchitectureDependencyRulesTests.cpp` and `tests/Engine/EngineLifecycleContractTests.cpp` for the full list.

### 5.2 New Tests to Add

#### 5.2.1: `DeliveryMustNotAccessEngineStatePrivateMembers`

```cpp
TEST(ArchitectureDependencyRules, DeliveryMustNotAccessEngineStatePrivateMembers) {
    // Check that delivery code does not access EngineState private members
    // through the allowlist pattern. All access should go through state services.
    const auto violations = findTokenViolations(
        repoRoot,
        fs::path{"src/Editor"},
        {
            "engineState_->scene",
            "engineState_->selectedEntity",
            "engineState_->cameraEntity",
            "engineState_->skybox",
            "engineState_->showSkybox",
            "engineState_->showGrid",
            "engineState_->showDebugObjects",
            "engineState_->physicsSimulationRunning",
            "engineState_->solidGroundEnabled",
            "engineState_->skySettings",
            "engineState_->postProcessPush",
            "engineState_->modelRenderSystem",
            "engineState_->shadowSystem",
            "engineState_->lightSystem",
            "engineState_->iblSystem",
            "engineState_->animationSystem",
            "engineState_->physicsSystem",
            "engineState_->joltPhysicsSystem",
        });
    EXPECT_TRUE(violations.empty())
        << "Delivery should not access EngineState private members directly: "
        << joinViolations(violations);
}
```

#### 5.2.2: `DomainMustNotDependOnInfrastructure`

```cpp
TEST(ArchitectureDependencyRules, DomainMustNotDependOnInfrastructure) {
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include/Engine/Scene"},
        {
            "Engine/Graphics/",
            "Engine/Systems/IBL/",
            "EngineSceneIO/",
            "ModelLib/",
        });
    EXPECT_TRUE(violations.empty())
        << "Domain headers must not depend on infrastructure: "
        << joinViolations(violations);
}
```

#### 5.2.3: `ApplicationUseCasesMustOnlyDependOnPortsAndDomain`

```cpp
TEST(ArchitectureDependencyRules, ApplicationUseCasesMustOnlyDependOnPortsAndDomain) {
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include/Engine/Application/UseCases"},
        {
            "Engine/Graphics/",
            "Engine/Systems/",
            "EngineSceneIO/",
            "ModelLib/",
            "Editor/",
            "Engine/EngineState.hpp",
        });
    EXPECT_TRUE(violations.empty())
        << "Use case headers must only depend on ports and domain types: "
        << joinViolations(violations);
}
```

#### 5.2.4: `AllPortsHaveImplementations`

```cpp
TEST(ArchitectureDependencyRules, AllPortsHaveImplementations) {
    // Verify that every port in Application/Ports has a corresponding adapter
    // in Editor/Infrastructure. This ensures ports are not abandoned.
    const fs::path repoRoot = findRepoRoot();
    ASSERT_FALSE(repoRoot.empty());

    const fs::path portsDir = repoRoot / "include/Engine/Application/Ports";
    const fs::path adaptersDir = repoRoot / "include/Editor/Infrastructure";

    std::set<std::string> ports;
    std::set<std::string> implementations;

    for (const auto& entry : fs::recursive_directory_iterator(portsDir)) {
        if (entry.path().extension() == ".hpp") {
            ports.insert(entry.path().filename().string());
        }
    }

    for (const auto& entry : fs::recursive_directory_iterator(adaptersDir)) {
        if (entry.path().extension() == ".hpp") {
            implementations.insert(entry.path().filename().string());
        }
    }

    for (const auto& port : ports) {
        // Port name should have a corresponding adapter
        // e.g., IScenePersistencePort -> ScenePersistenceAdapter
        const std::string adapterName = port.substr(1); // Remove 'I' prefix
        const std::string adapter = adapterName + "Adapter";
        // Note: not all ports may have adapters (some are internal)
        // This test is informational
    }
}
```

#### 5.2.5: `RenderPassesMustNotIncludeDeliveryHeaders`

```cpp
TEST(ArchitectureDependencyRules, RenderPassesMustNotIncludeDeliveryHeaders) {
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"include/Engine/Graphics/Passes"},
        {"Editor/"});
    EXPECT_TRUE(violations.empty())
        << "Render pass headers must not include delivery headers: "
        << joinViolations(violations);
}
```

#### 5.2.6: `RenderPassImplementationsMustNotIncludeDeliveryHeaders`

```cpp
TEST(ArchitectureDependencyRules, RenderPassImplementationsMustNotIncludeDeliveryHeaders) {
    const auto violations = findIncludeViolations(
        repoRoot,
        fs::path{"src/Engine/Graphics/Passes"},
        {"Editor/"});
    EXPECT_TRUE(violations.empty())
        << "Render pass implementations must not include delivery headers: "
        << joinViolations(violations);
}
```

#### 5.2.7: `EngineStateMustNotIncludeVulkanTypesDirectly`

```cpp
TEST(ArchitectureDependencyRules, EngineStateMustNotIncludeVulkanTypesDirectly) {
    const std::string engineStateHpp = readWholeFile(repoRoot / "include/Engine/EngineState.hpp");
    ASSERT_FALSE(engineStateHpp.empty());

    // EngineState.hpp should not include vulkan headers
    const auto vulkanViolations = findIncludeViolations(
        repoRoot,
        fs::path{"include/Engine"},
        {"vulkan/"});
    EXPECT_TRUE(vulkanViolations.empty())
        << "Engine headers should not include Vulkan SDK headers directly: "
        << joinViolations(vulkanViolations);
}
```

#### 5.2.8: `StateServicesMustNotExposeConcreteSystems`

```cpp
TEST(ArchitectureDependencyRules, StateServicesShouldNotExposeConcreteSystems) {
    // Verify that state services return views/interfaces, not raw system pointers
    // where possible. AnimationRuntimeService and PhysicsRuntimeService are
    // transitional - they return concrete system pointers.
    const std::string stateServicesCpp = readWholeFile(repoRoot / "src/Engine/State/StateServices.cpp");
    ASSERT_FALSE(stateServicesCpp.empty());

    // This is informational - the narrow state services already exist
    // The goal is to eventually replace concrete pointer returns with views
}
```

### 5.3 Test Execution Strategy

Run architecture tests on every CI build:
```bash
xmake run Tests -- --gtest_filter="ArchitectureDependencyRules.*"
```

Fail the build if any architecture test fails. This ensures the architecture is enforced automatically.

---

## 6. Risk Assessment

### 6.1 High-Risk Items

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| **EnvironmentLightingAdapter port expansion is too large** | High | High | Split into smaller, focused ports. Start with `ISkyboxAccessPort` and `IDescriptorAccessPort` as separate concerns. |
| **Render pass interface changes break build** | High | Medium | Make changes incrementally. Each pass can be migrated independently. Keep the old constructor as deprecated during transition. |
| **CompositionPass UIManager dependency removal** | Medium | Medium | Create `ICompositionPort` early. Test that the adapter correctly delegates to UIManager. |
| **State service pattern creates too many layers** | Medium | Low | Keep state services narrow. Each service should have exactly one purpose. |

### 6.2 Medium-Risk Items

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| **IDescriptorAccessPort is too broad** | High | Medium | Split into `IDescriptorPoolPort` and `IDescriptorSetPort`. Or create a `IDescriptorWriterPort` that wraps the DescriptorWriter pattern. |
| **Use case proliferation** | Medium | Low | Group related use cases into a use case module. Don't create a use case for every single operation. |
| **EngineStateVulkan split creates confusion** | Low | Low | Clear documentation in the new header. Keep it minimal. |
| **Allowlist removal causes test failures** | Medium | Medium | Remove allowlists incrementally. Each phase should remove one allowlisted file at a time. |

### 6.3 Low-Risk Items

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| **PhysicsRuntimeAdapter direct dependency is acceptable** | Low | Low | Document the rationale. Keep it as-is. |
| **SceneRuntimeState DTO becomes bloated** | Low | Low | Keep it minimal. Split if it grows too large. |
| **Missing use cases are not critical** | Low | Low | Prioritize based on editor usage frequency. |

### 6.4 Mitigation Strategies

#### Strategy 1: Incremental Allowlist Removal

Remove one allowlisted file at a time. After each removal:
1. Fix the violation
2. Run all architecture tests
3. Confirm all tests pass
4. Commit the change

#### Strategy 2: Deprecated Constructor Pattern

For render pass interface changes, use deprecated constructors:
```cpp
// New constructor
RenderPass(RenderingStateView rendering, SceneRuntimeStateView scene);

// Deprecated constructor (keep for transition)
RenderPass(EngineState* engineState) : RenderPass(engineStateToView(engineState)) {}
```

This allows gradual migration without breaking the build.

#### Strategy 3: Port-First Design

When adding new use cases, define the port first, then implement the use case. This ensures the application layer defines the contract before infrastructure implements it.

#### Strategy 4: Architecture Test as Gate

Treat architecture test failures as build failures. This ensures the architecture is enforced automatically and prevents regression.

---

## 7. Appendix: File Inventory

### 7.1 Files to Create

| File | Purpose | Phase |
|------|---------|-------|
| `include/Engine/Application/Ports/ISceneRuntimeAccessPort.hpp` | Scene runtime state access port | Phase 1 |
| `include/Editor/Infrastructure/SceneRuntimeAccessAdapter.hpp` | Adapter for ISceneRuntimeAccessPort | Phase 1 |
| `src/Editor/Infrastructure/SceneRuntimeAccessAdapter.cpp` | Adapter implementation | Phase 1 |
| `include/Engine/Application/Ports/IAnimationAccessPort.hpp` | Animation system access port | Phase 1 |
| `include/Editor/Infrastructure/AnimationAccessAdapter.hpp` | Adapter for IAnimationAccessPort | Phase 1 |
| `src/Editor/Infrastructure/AnimationAccessAdapter.cpp` | Adapter implementation | Phase 1 |
| `include/Engine/Application/Ports/IPostProcessingAccessPort.hpp` | Post-processing recreation port | Phase 1 |
| `include/Editor/Infrastructure/PostProcessingAccessAdapter.hpp` | Adapter for IPostProcessingAccessPort | Phase 1 |
| `src/Editor/Infrastructure/PostProcessingAccessAdapter.cpp` | Adapter implementation | Phase 1 |
| `include/Engine/Application/Ports/ICompositionPort.hpp` | Composition/UI port | Phase 2 |
| `include/Editor/Infrastructure/CompositionPortAdapter.hpp` | Adapter for ICompositionPort | Phase 2 |
| `src/Editor/Infrastructure/CompositionPortAdapter.cpp` | Adapter implementation | Phase 2 |
| `include/Engine/Application/Ports/IRenderContextPort.hpp` | RenderContext abstraction port | Phase 2 |
| `include/Editor/Infrastructure/RenderContextPortAdapter.hpp` | Adapter for IRenderContextPort | Phase 2 |
| `src/Editor/Infrastructure/RenderContextPortAdapter.cpp` | Adapter implementation | Phase 2 |
| `include/Engine/Application/Ports/ISkyboxAccessPort.hpp` | Skybox access port | Phase 3 |
| `include/Editor/Infrastructure/SkyboxAccessAdapter.hpp` | Adapter for ISkyboxAccessPort | Phase 3 |
| `src/Editor/Infrastructure/SkyboxAccessAdapter.cpp` | Adapter implementation | Phase 3 |
| `include/Engine/Application/Ports/IDescriptorAccessPort.hpp` | Descriptor access port | Phase 3 |
| `include/Editor/Infrastructure/DescriptorAccessAdapter.hpp` | Adapter for IDescriptorAccessPort | Phase 3 |
| `src/Editor/Infrastructure/DescriptorAccessAdapter.cpp` | Adapter implementation | Phase 3 |
| `include/Engine/Application/Ports/ISceneAccessPort.hpp` | Scene access port | Phase 3 |
| `include/Editor/Infrastructure/SceneAccessAdapter.hpp` | Adapter for ISceneAccessPort | Phase 3 |
| `src/Editor/Infrastructure/SceneAccessAdapter.cpp` | Adapter implementation | Phase 3 |
| `include/Engine/Application/UseCases/AddEntityUseCase.hpp` | Add entity use case | Phase 4 |
| `src/Engine/Application/UseCases/AddEntityUseCase.cpp` | Add entity implementation | Phase 4 |
| `include/Engine/Application/UseCases/DeleteEntityUseCase.hpp` | Delete entity use case | Phase 4 |
| `src/Engine/Application/UseCases/DeleteEntityUseCase.cpp` | Delete entity implementation | Phase 4 |
| `include/Engine/Application/UseCases/SetActiveCameraUseCase.hpp` | Set active camera use case | Phase 4 |
| `src/Engine/Application/UseCases/SetActiveCameraUseCase.cpp` | Set active camera implementation | Phase 4 |
| `include/Engine/Application/UseCases/LoadModelIntoSceneUseCase.hpp` | Load model use case | Phase 4 |
| `src/Engine/Application/UseCases/LoadModelIntoSceneUseCase.cpp` | Load model implementation | Phase 4 |
| `include/Engine/Application/UseCases/TogglePhysicsSimulationUseCase.hpp` | Toggle physics use case | Phase 4 |
| `src/Engine/Application/UseCases/TogglePhysicsSimulationUseCase.cpp` | Toggle physics implementation | Phase 4 |
| `include/Engine/Application/UseCases/ChangeSkyboxSettingsUseCase.hpp` | Change skybox settings use case | Phase 4 |
| `src/Engine/Application/UseCases/ChangeSkyboxSettingsUseCase.cpp` | Change skybox settings implementation | Phase 4 |
| `include/Engine/Application/UseCases/ChangeShadowSettingsUseCase.hpp` | Change shadow settings use case | Phase 4 |
| `src/Engine/Application/UseCases/ChangeShadowSettingsUseCase.cpp` | Change shadow settings implementation | Phase 4 |
| `include/Engine/Application/UseCases/ApplyTransformUseCase.hpp` | Apply transform use case | Phase 4 |
| `src/Engine/Application/UseCases/ApplyTransformUseCase.cpp` | Apply transform implementation | Phase 4 |
| `include/Engine/EngineStateVulkan.hpp` | Vulkan-specific EngineState | Phase 5 |
| `tests/Engine/Architecture/DependencyGraphTests.cpp` | Dependency graph tests | Phase 6 |
| `tests/Engine/Architecture/UseCaseTests.cpp` | Use case tests | Phase 6 |
| `tests/Engine/Architecture/PortTests.cpp` | Port tests | Phase 6 |
| `docs/architecture-decisions/001-clean-architecture-migration.md` | ADR for migration | Phase 6 |
| `docs/architecture-decisions/002-engine-state-as-coordinator.md` | ADR for EngineState | Phase 6 |
| `docs/architecture-decisions/003-state-services-pattern.md` | ADR for state services | Phase 6 |
| `docs/architecture-decisions/004-render-pass-interface.md` | ADR for render passes | Phase 6 |

### 7.2 Files to Modify

| File | Purpose | Phase |
|------|---------|-------|
| `include/Engine/Application/Ports/IEnvironmentLightingPort.hpp` | Expand port interface | Phase 3 |
| `include/Editor/Infrastructure/EnvironmentLightingAdapter.hpp` | Update constructor | Phase 3 |
| `src/Editor/Infrastructure/EnvironmentLightingAdapter.cpp` | Remove EngineState deps | Phase 3 |
| `include/Engine/Graphics/Passes/UpdatePass.hpp` | Change constructor | Phase 2 |
| `include/Engine/Graphics/Passes/ShadowPass.hpp` | Change constructor | Phase 2 |
| `include/Engine/Graphics/Passes/OffscreenPass.hpp` | Change constructor | Phase 2 |
| `include/Engine/Graphics/Passes/CompositionPass.hpp` | Change constructor | Phase 2 |
| `include/Engine/Graphics/Passes/ComputePass.hpp` | Change constructor | Phase 2 |
| `include/Engine/Graphics/Passes/DepthPrepass.hpp` | Change constructor | Phase 2 |
| `src/Engine/Graphics/Passes/UpdatePass.cpp` | Update implementation | Phase 2 |
| `src/Engine/Graphics/Passes/ShadowPass.cpp` | Update implementation | Phase 2 |
| `src/Engine/Graphics/Passes/OffscreenPass.cpp` | Update implementation | Phase 2 |
| `src/Engine/Graphics/Passes/CompositionPass.cpp` | Update implementation | Phase 2 |
| `src/Engine/Graphics/Passes/ComputePass.cpp` | Update implementation | Phase 2 |
| `src/Engine/Graphics/Passes/DepthPrepass.cpp` | Update implementation | Phase 2 |
| `include/Engine/EngineState.hpp` | Remove Vulkan includes | Phase 5 |
| `src/Engine/EngineState.cpp` | Include Vulkan header | Phase 5 |
| `include/Editor/app.hpp` | Add port members | Phase 1 |
| `src/Editor/app.cpp` | Replace EngineState access | Phase 1 |
| `tests/Engine/ArchitectureDependencyRulesTests.cpp` | Update allowlists, add tests | Phase 6 |
| `xmake.lua` | Add architecture test target | Phase 6 |

### 7.3 Files to Delete (Eventually)

| File | Reason | Phase |
|------|--------|-------|
| `include/Engine/State/RuntimeSettingsBindingService.hpp` | Replace with use cases | Phase 4+ |
| `src/Engine/State/RuntimeSettingsBindingService.cpp` | Replace with use cases | Phase 4+ |

**Note:** RuntimeSettingsBindingService is a transitional application-layer service. It can be kept until all its operations are covered by use cases, then removed in favor of use case composition.

---

## 8. Phase Dependencies and Execution Order

```
Phase 1 (Delivery -> EngineState decoupling)
    |
    +-- Phase 2 (Render pass interface changes)
    |       |
    |       +-- Phase 3 (Infrastructure adapter port expansion)
    |               |
    |               +-- Phase 4 (Missing use cases)
    |                       |
    |                       +-- Phase 5 (EngineState refactoring)
    |                               |
    |                               +-- Phase 6 (Final cleanup)
    |
    +-- Phase 3 can run in parallel with Phase 1 (no dependency)
    +-- Phase 4 can run in parallel with Phase 2 (no dependency)
    +-- Phase 6 depends on all previous phases
```

**Recommended execution order:**
1. Phase 1 (highest impact, foundational)
2. Phase 3 (parallel with Phase 4)
3. Phase 4 (parallel with Phase 3)
4. Phase 2 (requires Phase 1 to be mostly complete)
5. Phase 5 (can run anytime after Phase 1)
6. Phase 6 (depends on all previous)

---

## 9. Success Criteria

The migration is complete when:

1. **Zero allowlists:** All architecture test allowlists are empty
2. **Zero direct EngineState access:** Delivery code has no direct EngineState member access
3. **Zero infrastructure -> delivery includes:** No infrastructure code includes Editor headers
4. **All editor commands use use cases:** Every significant editor action goes through a use case
5. **All ports have implementations:** Every port in Application/Ports has a corresponding adapter
6. **EngineState has no Vulkan types:** EngineState.hpp has no Vulkan SDK includes
7. **All architecture tests pass:** Every architecture test passes with zero violations
8. **All existing tests pass:** No regression in unit tests or integration tests
9. **Build succeeds:** The project builds without warnings related to architecture changes
10. **ADR documentation:** Architecture decision records are written for all major decisions

---

## 10. Glossary

| Term | Definition |
|------|-----------|
| **Clean Architecture** | A software design philosophy by Robert C. Martin that emphasizes separation of concerns and dependency inversion |
| **Hexagonal Architecture** | An architecture pattern that isolates the core application logic from external concerns through ports and adapters |
| **Port** | An interface defined by the application layer that specifies how the application interacts with the outside world |
| **Adapter** | A concrete implementation of a port that bridges the application layer to specific infrastructure |
| **Use Case** | A specific business operation that the application supports, implemented as a class that orchestrates domain logic |
| **State View** | A lightweight struct that provides read-only access to a subset of EngineState, used to pass state through architecture boundaries |
| **State Service** | A class that provides controlled access to EngineState subsystems, returning state views |
| **Allowlist** | A temporary exception list in architecture tests that allows specific violations during migration |
| **Composition Root** | The point in the application where object graphs are assembled (in this case, App::init()) |
| **Dependency Inversion** | The principle that high-level modules should not depend on low-level modules; both should depend on abstractions |

---

*End of document.*

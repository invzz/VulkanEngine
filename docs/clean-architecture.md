# Clean Architecture (Target State)

## Layer Model

Delivery (Editor, tools, app bootstrap)
-> Application (use cases, orchestration)
-> Domain (scene/entities/components/rules)

Application
-> Ports (interfaces)
<- Infrastructure (Vulkan, filesystem, serializers, physics adapters)

Runtime systems should consume Domain models directly where possible and only cross infrastructure through ports.

## Current Mapping

- Domain: include/Engine/Scene, include/Engine/Core (domain-safe parts), component/value objects.
- Application: include/Engine/Application, src/Engine/Application.
- Ports: include/Engine/Application/Ports.
- Infrastructure: src/Engine/Graphics, src/EngineSceneIO, src/ModelLib and concrete adapters in include/Editor/Infrastructure + src/Editor/Infrastructure.
- Delivery: src/Editor, src/tools.

## First Refactor Slice Implemented

- Introduced ports:
  - IScenePersistencePort
  - IPhysicsRuntimePort
- Introduced use case:
  - LoadSceneUseCase
- Added infrastructure adapters:
  - ScenePersistenceAdapter (SceneSerializer adapter)
  - PhysicsRuntimeAdapter (Jolt runtime adapter)
- Wired Editor App through LoadSceneUseCase for startup and UI-triggered scene loading.

## Second Refactor Slice Implemented

- Added shared application runtime state object:
  - SceneRuntimeState
- Added SaveSceneUseCase and routed save callback through Application layer.
- Delivery now uses a single SceneRuntimeState builder (`App::sceneRuntimeState()`) for load workflows.
- Extended architecture dependency tests to enforce include-direction boundaries for Application and Domain layers.

## Third Refactor Slice Implemented

- Added `ReconcileSceneLoadUseCase` in Application layer to own post-load camera reconciliation.
- Removed direct camera-registry scan from `App::update`; Delivery now delegates post-load reconciliation to Application use case.
- Updated lifecycle contract tests to enforce wiring and execution of the new use case.

## Fourth Refactor Slice Implemented

- Added `IEnvironmentLightingPort` and `SyncEnvironmentLightingUseCase` in Application layer.
- Added `EnvironmentLightingAdapter` in Infrastructure to encapsulate skybox/IBL sync and deferred IBL descriptor-set refresh.
- Removed direct skybox/IBL orchestration from `App::update`; Delivery now delegates environment-lighting synchronization through Application use case.
- Added an architecture rule test to prevent direct post-load camera reconciliation logic from reappearing in Delivery.

## Fifth Refactor Slice Implemented

- Added `ISceneSelectionMaintenancePort` and `ProcessSceneSelectionMaintenanceUseCase` in Application layer.
- Moved delayed scene-deletion/selection maintenance orchestration from `App::update` behind the new Application use case.
- Added architecture rule test to prevent direct skybox loading, deferred-IBL descriptor rewrites, and related environment-lighting orchestration from reappearing in Delivery.

## Sixth Refactor Slice Implemented

- Extracted grouped EngineState state-view types into dedicated headers (`Engine/State/StateViews.hpp`).
- Added narrower state service wrappers (`RenderingStateService`, `SceneRuntimeService`, `InputStateService`, `ResourceStateService`) in `Engine/State/StateServices.hpp`.
- Exposed EngineState service accessors and adopted service-based access in App update/setup flow.
- Migrated EnvironmentLightingAdapter to consume the new service accessors.
- Added architecture guard test to keep grouped accessor usage confined to migration shims.
- Extended lifecycle tests to enforce presence of the new service accessors.

## Seventh Refactor Slice Implemented

- Migrated `SettingsPanel` and `IBLPanel` from direct EngineState runtime getters to `renderingService()`/`sceneRuntimeService()` state views.
- Added architecture guard test to keep migrated panels on state-service based runtime queries.

## Eighth Refactor Slice Implemented

- Migrated `ScenePanel` scene access from direct `getScene()` chains to `sceneRuntimeService().view().scene` with local scene/registry references.
- Added architecture guard test to keep `ScenePanel` on scene-runtime service based access and prevent `getScene()` regressions.

## Ninth Refactor Slice Implemented

- Migrated `SettingsPanel` and `ScenePanel` resource-manager access from `getResourceManager()` to `resourceService().view().resourceManager`.
- Strengthened panel architecture guard tests to prevent `getResourceManager()` regressions in migrated panels.

## Tenth Refactor Slice Implemented

- Added a UI-wide architecture guard (`EditorUiShouldUseNarrowServicesInsteadOfLegacyEngineStateGetters`) that scans all `src/Editor/ui/*.cpp` files for legacy grouped EngineState getter usage with an explicit allowlist gate.
- Migrated `App` scene and rendering wiring from legacy getters to service/view-based access for scene/runtime bindings.
- Removed unused legacy EngineState getters: `getScene()`, `getIBLSystem()`, and `getResourceManager()`.

## Eleventh Refactor Slice Implemented

- Migrated render/update pass hotspots (`UpdatePass`, `ComputePass`, `DepthPrepass`, `OffscreenPass`, `ShadowPass`, `CompositionPass`) to `inputService()` / `renderingService()` / `sceneRuntimeService()` / `systemServices()` based access.
- Added a render-pass architecture guard (`RenderPassesShouldUseStateServicesInsteadOfLegacyEngineStateGetters`) to block regressions back to legacy EngineState getters in `src/Engine/Graphics/Passes`.
- Reduced legacy EngineState getter surface further by removing now-unused runtime-system getters (`getRenderContext`, `getModelRenderSystem`, `getAnimationSystem`, `getObjectSelectionSystem`, `getInputSystem`, `getCameraSystem`, `getColliderDebugRenderSystem`, `getShadowSystem`, `getLightSystem`, `getGridRenderSystem`, `getDeferredLightingSystem`, `getPostProcessingSystem`).

## Boundary Rules

- Domain must not include Editor, Vulkan, filesystem, or serializer implementations.
- Application depends on Domain + Ports only.
- Infrastructure implements Ports and may depend on external frameworks.
- Delivery composes concrete Infrastructure into Application use cases.

## Next Refactor Slices

1. Add ports for input/render commands where editor currently touches runtime systems directly.
2. Split EngineState into narrower state services (render/input/scene runtime). (in progress)
3. Add Infrastructure-side boundary tests for adapter and external-dependency placement.
4. Migrate SceneSerializer runtime settings bindings behind Application workflows.

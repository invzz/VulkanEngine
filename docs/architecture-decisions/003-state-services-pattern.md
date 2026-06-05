# ADR-003: State Services Pattern

**Status:** Accepted  
**Date:** 2026-06-03

## Problem

Render passes and delivery code needed access to EngineState subsystems, but direct member access violated Clean Architecture boundaries. Long parameter lists were impractical for the 15+ systems.

## Decision

Introduce **State Services** — narrow accessor classes that return **State Views** (lightweight DTOs):

```cpp
// State Service (returned by value, holds EngineState reference)
class RenderingStateService {
    EngineState& engineState_;
public:
    RenderingStateView view() const;  // returns DTO
};

// State View (pure data, no behavior)
struct RenderingStateView {
    ModelRenderSystem* modelRenderSystem;
    ShadowSystem* shadowSystem;
    // ... pointers to subsystems
    bool* showSkybox;
    // ... mutable state references
};
```

## Usage

```cpp
// In render loop or pass construction:
auto rendering = engineState.renderingService().view();
auto sceneRuntime = engineState.sceneRuntimeService().view();
```

## Why Services Return Views by Value?

- **Zero allocation:** Views are trivial structs with pointers
- **Snapshot semantics:** View captures state at construction time
- **No lifetime issues:** Views contain raw pointers owned by EngineState
- **Compile-time safety:** Service methods are inline, views are POD

## Services Implemented

| Service | View | Purpose |
|---------|------|---------|
| `RenderingStateService` | `RenderingStateView` | Render systems + flags |
| `SceneRuntimeService` | `SceneRuntimeStateView` | Scene, entities, camera |
| `InputStateService` | `InputStateView` | Input devices, systems |
| `ResourceStateService` | `ResourceStateView` | Resources, descriptors |
| `AnimationRuntimeService` | `AnimationSystem*` | Animation system |
| `PhysicsRuntimeService` | `JoltPhysicsSystem*` | Physics system |

## Consequences

- Render passes accept state views, not EngineState pointers
- Architecture test `RenderPassesShouldUseStateServicesInsteadOfLegacyEngineStateGetters` enforces adoption
- Legacy getters (`getScene()`, `getModelRenderSystem()`) are deprecated

# Clean Architecture Refactoring Plan

This document is the execution roadmap for finishing the clean architecture migration.
It complements [clean-architecture.md](./clean-architecture.md), which records completed slices.

## Target End State

1. Delivery (`src/Editor`, `src/tools`) only composes use cases and ports.
2. Application (`include/Engine/Application`, `src/Engine/Application`) owns orchestration and policies.
3. Domain (`include/Engine/Scene`, domain-safe Engine core) has no dependency on delivery or infrastructure concerns.
4. Infrastructure (graphics, serialization, model I/O, adapters) implements ports and remains replaceable.
5. `EngineState` is no longer a broad mutable gateway; access is narrowed through explicit services/contracts.
6. Architecture tests enforce boundaries with minimal or zero migration allowlists.

## Baseline Snapshot (June 2026)

1. Slices 1-11 are completed and documented in [clean-architecture.md](./clean-architecture.md).
2. UI and render passes are mostly migrated to state-service-based access.
3. Transitional seams still exist:
- `systemServices()` usage in hot paths.
- Direct Jolt system wiring via `getJoltPhysicsSystem()` in app composition.
- `SceneSerializer::setRuntimeSettingsBindings(...)` called directly from delivery.
- Architecture allowlist for grouped accessors still includes pass files.

## Definition Of Done

1. No direct grouped accessor calls (`renderingState`, `sceneState`, `inputState`, `resourceState`, `systemServices`) outside explicit migration shims, ideally none.
2. No direct runtime-system getter usage from delivery and render passes.
3. Scene runtime settings persistence flows through Application use cases/ports, not delivery wiring.
4. Architecture tests pass with empty or near-empty temporary allowlists.
5. Full test suite (`xmake run Tests`) and editor smoke run (`xmake run Editor`) pass.

## Workstream A: EngineState Decomposition

### A1. Replace transitional `systemServices()` usage

Tasks:
- Introduce dedicated narrow services for remaining broad system groups, for example:
  - `AnimationRuntimeService`
  - `DebugRenderService` (camera/collider/light debug rendering)
  - `PhysicsRuntimeService` (Jolt sync/update surface)
- Migrate remaining callsites currently using `systemServices()` in:
  - `src/Editor/app.cpp`
  - `src/Engine/Graphics/Passes/UpdatePass.cpp`
  - `src/Engine/Graphics/Passes/OffscreenPass.cpp`
  - `src/Engine/Graphics/Passes/ComputePass.cpp`

Acceptance criteria:
- `rg "systemServices\(" src` returns no production callsites.
- Grouped accessor allowlist entry for pass files is removed.

### A2. Remove obsolete EngineState getters and tighten visibility

Tasks:
- Remove any getter replaced by narrow services.
- Keep only value/reference accessors that are truly stateful primitives and cannot yet be ported.
- Move remaining grouped state builders behind service internals (private + friendship or equivalent).

Acceptance criteria:
- Getter surface in `include/Engine/EngineState.hpp` is intentionally small and documented.
- Lifecycle tests assert new minimal accessor surface.

## Workstream B: Delivery To Application Orchestration

### B1. Move serializer runtime binding orchestration out of delivery

Tasks:
- Introduce application-level abstraction for runtime settings persistence binding.
- Delivery should no longer build `RuntimeSettingsBindings` directly.
- Implement infrastructure adapter around `SceneSerializer` for settings load/save concerns.

Acceptance criteria:
- `sceneSerializer.setRuntimeSettingsBindings(...)` is not called from `src/Editor/app.cpp`.
- Settings persistence behavior remains equivalent in save/load workflows.

### B2. Continue port extraction for editor-driven runtime commands

Tasks:
- Identify editor actions still directly invoking runtime systems.
- Define use cases/ports for those actions (input, debug draw toggles, physics controls as needed).
- Replace direct delivery branching with use case execution.

Acceptance criteria:
- Delivery callbacks in `UIManager` setup are orchestration-only.
- Application owns side-effect sequencing and policy decisions.

## Workstream C: Physics Boundary Hardening

### C1. Remove direct Jolt pointer flow from delivery composition

Tasks:
- Replace `getJoltPhysicsSystem()` wiring with a dedicated physics runtime service or adapter input from state services.
- Ensure panels/features that need physics interactions depend on abstractions, not concrete Jolt system pointers.

Acceptance criteria:
- No delivery constructor path directly requests concrete Jolt system from `EngineState`.
- Physics behaviors are covered through application/infrastructure seams.

## Workstream D: Infrastructure Boundary Enforcement

### D1. Expand architecture tests for adapter boundaries

Tasks:
- Add tests enforcing that `include/Editor/Infrastructure` depends only on ports/contracts plus minimal runtime DTOs.
- Add tests enforcing `src/Editor/Infrastructure` does not depend on delivery UI panels.
- Add targeted guard tests per critical adapter (scene persistence, physics runtime, environment lighting).

Acceptance criteria:
- New tests fail on intentional dependency violations.
- Existing boundary allowlists are reduced or justified inline.

## Workstream E: Domain Purity And Shared Contracts

### E1. Audit domain-facing types crossing layers

Tasks:
- Review DTO/state objects used by application and adapters.
- Keep domain objects free from infrastructure-specific fields.
- Move mixed concerns to application contracts where needed.

Acceptance criteria:
- Domain headers have no infrastructure include drift.
- Architecture tests for domain include boundaries remain green.

## Workstream F: Regression Guards And Test Strategy

### F1. Guard expansion

Tasks:
- Keep UI-wide and render-pass-wide token guards.
- Add a guard for delivery-level serializer binding orchestration once migrated.
- Add guards for disallowed `EngineState` getter names as they are removed.

Acceptance criteria:
- Refactor regressions are caught by focused test filters.

### F2. Validation cadence per slice

Tasks:
- For each slice, run narrow filters first, then broader suites:
  - `ArchitectureDependencyRules.*`
  - `EngineLifecycleContracts.*`
  - targeted behavior tests for touched features.
- Run full tests at phase boundaries.

Acceptance criteria:
- Every merged slice has executable validation evidence.

## Execution Phases

### Phase 1: Remove `systemServices` transitional usage

Scope:
- Complete A1 in passes + app.

Exit criteria:
- No production `systemServices()` usage.
- Grouped accessor allowlist shrinks to shims only.

### Phase 2: EngineState API minimization

Scope:
- Complete A2.

Exit criteria:
- Legacy getters removed or justified.
- Engine lifecycle contract tests updated and green.

### Phase 3: Serializer/runtime settings workflow migration

Scope:
- Complete B1.

Exit criteria:
- Delivery no longer sets runtime bindings directly.
- Save/load behavior parity validated.

### Phase 4: Remaining editor command ports

Scope:
- Complete B2 and C1.

Exit criteria:
- Delivery callbacks become use-case oriented.
- Direct concrete runtime system dependencies are removed from delivery.

### Phase 5: Boundary and contract hardening

Scope:
- Complete D1, E1, F1.

Exit criteria:
- Architecture tests cover all critical boundaries and have minimal allowlists.

### Phase 6: Stabilization and closeout

Scope:
- Complete F2 with full-suite validation.
- Update [clean-architecture.md](./clean-architecture.md) with completed slices and final status.

Exit criteria:
- Full tests green.
- Editor smoke run green.
- Plan checklist fully completed.

## Master Checklist

- [ ] A1 Replace `systemServices()` usage with narrower services.
- [ ] A2 Minimize and lock down `EngineState` public accessor surface.
- [ ] B1 Move runtime settings binding orchestration from delivery to application workflow.
- [ ] B2 Extract remaining editor runtime command ports/use cases.
- [ ] C1 Decouple delivery from concrete Jolt runtime wiring.
- [ ] D1 Expand and tighten infrastructure boundary tests.
- [ ] E1 Audit and harden domain-facing contracts.
- [ ] F1 Add regression guards for newly enforced seams.
- [ ] F2 Execute full validation and close out allowlists.

## Risks And Mitigations

1. Risk: Over-splitting services introduces noisy abstractions.
Mitigation: Add services only where they remove a concrete coupling and can be enforced with a guard test.

2. Risk: Runtime behavior drift during serializer binding migration.
Mitigation: Keep behavior parity checks around scene save/load and runtime toggles.

3. Risk: Hidden ECS validity assumptions in UI workflows.
Mitigation: Preserve safety checks around entity validity/component presence during migrations.

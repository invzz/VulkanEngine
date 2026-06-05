# ADR-001: Clean Architecture Migration

**Status:** Accepted  
**Date:** 2026-06-03  
**Context:** VulkanEngine codebase refactoring  
**Decision:** Adopt four-layer Clean Architecture (Delivery → Application → Domain → Infrastructure)

## Problem

The VulkanEngine codebase had tight coupling between the Editor (delivery layer) and Engine internals (EngineState). Delivery code accessed EngineState members directly, making it impossible to change engine internals without touching delivery code.

## Decision

Adopt a four-layer architecture:

1. **Domain** — Pure entities, components, scene graph (zero infrastructure knowledge)
2. **Application** — Use cases, ports (interfaces), orchestration logic
3. **Infrastructure** — Vulkan, filesystem, serializers, physics adapters
4. **Delivery** — Editor UI, app bootstrap, composition root

Dependency direction: outer layers depend on inner layers through ports (interfaces).

## Consequences

- **Positive:** Engine internals can change without delivery changes. Architecture is enforced by automated tests.
- **Negative:** More indirection. Ports and adapters add boilerplate. Some performance-critical paths (render loop) still use state views for efficiency.
- **Trade-off:** State services provide controlled access to EngineState without full decoupling — a pragmatic middle ground.

## Implementation Status

- ✅ Phase 0-3 complete (foundation, render passes, infrastructure adapters)
- ✅ app.cpp decoupled from direct EngineState member access
- ✅ 30 architecture tests enforce boundaries
- ⏳ Domain-bleed in scene components (transitional allowlist)

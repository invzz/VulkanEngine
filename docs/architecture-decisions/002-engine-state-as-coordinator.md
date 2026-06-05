# ADR-002: EngineState as Application-Layer Coordinator

**Status:** Accepted  
**Date:** 2026-06-03

## Problem

EngineState was a "god object" owning 15+ concrete systems, descriptor pools, scene state, and runtime settings. Every layer (delivery, infrastructure, render passes) accessed it directly, creating a single point of coupling.

## Decision

EngineState is an **application-layer coordinator** that:
- Owns system lifecycles via `SystemRegistry`
- Provides **state services** (`RenderingStateService`, `SceneRuntimeService`, etc.) returning **state views** (DTOs)
- Exposes **ref accessors** (`showSkyboxRef()`, `skySettingsRef()`) for mutable runtime state
- Is NOT a domain type — it coordinates infrastructure through ports

## Why Not Full Decoupling?

`std::unique_ptr<SystemType>` members require complete types at destruction time. Moving them to a pimpl detail header would require:
1. Custom deleters or explicit destruction in `.cpp`
2. All state service implementations to include the detail header
3. Significant refactoring of the initialization sequence

The pragmatic approach: keep system includes in `EngineState.hpp`, but enforce that delivery code accesses state through services/ports, not direct members.

## State Access Patterns

| Pattern | Example | Acceptable |
|---------|---------|------------|
| Service accessor | `engineState.renderingService().view()` | ✅ |
| Ref accessor | `engineState.showSkyboxRef()` | ✅ |
| Direct member | `engineState_->modelRenderSystem` | ❌ |
| Legacy getter | `engineState.getModelRenderSystem()` | ❌ |

## Consequences

- Delivery code uses service accessors, not direct member access
- Architecture tests enforce the boundary (`HotPathsUseEngineStateSystemAccessorsNotDirectMembers`)
- Render passes receive state views, not EngineState pointers

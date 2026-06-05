# ADR-004: Render Pass Interface Design

**Status:** Accepted  
**Date:** 2026-06-03

## Problem

Render passes (`UpdatePass`, `ShadowPass`, `CompositionPass`, etc.) previously took `EngineState*` directly, creating tight coupling between infrastructure (render passes) and the application-layer coordinator.

## Decision

Render passes accept **state views** and **port interfaces** instead of `EngineState*`:

```cpp
// Before (coupled):
class ShadowPass {
    EngineState* engineState_;
};

// After (decoupled):
class ShadowPass {
    RenderingStateView rendering_;
    SceneRuntimeStateView scene_;
    IRenderContextPort* renderContext_;
};
```

## Port Interfaces for Render Passes

| Port | Purpose | Used By |
|------|---------|---------|
| `IRenderContextPort` | Light buffers, UBOs, descriptors | ShadowPass, OffscreenPass |
| `ICompositionPort` | UI rendering delegation | CompositionPass |
| `IAnimationAccessPort` | Morph target manager | ComputePass |

## Why Not Full Port Abstraction?

Render passes need direct access to Vulkan resources (descriptor sets, command buffers, pipeline objects). Creating ports for every Vulkan operation would add excessive indirection to performance-critical paths. The pragmatic approach:

1. State views provide read-only access to system pointers
2. Ports abstract delivery-specific concerns (UI, render context)
3. Vulkan resource access remains direct in infrastructure layer

## Architecture Test Enforcement

- `RenderPassesShouldNotDependOnEngineState` — verifies no `EngineState*` in pass headers
- `RenderPassesMustNotIncludeDeliveryHeaders` — verifies no `Editor/` includes
- `RenderPassesShouldUseStateServicesInsteadOfLegacyEngineStateGetters` — enforces service adoption

## Consequences

- Render passes are now infrastructure-layer code with clean boundaries
- CompositionPass no longer includes `Editor/ui/UIManager.hpp` (was in allowlist)
- ShadowPass/OffscreenPass no longer include `Editor/RenderContext.hpp` (was in allowlist)
- Architecture test allowlist for Engine→Editor includes is now empty

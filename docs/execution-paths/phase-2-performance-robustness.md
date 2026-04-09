# Phase 2 Execution Path: Performance and Robustness

Timeline: Days 31-60

Status: Active (started April 9, 2026)

## Objective
Eliminate common frame stalls and descriptor churn while improving fallback behavior.

## Kickoff Checklist
- [ ] Week 5 scope lock: confirm async loading MVP and non-goals.
- [ ] Add baseline measurement notes for scene import frame-time spikes.
- [ ] Start ResourceManager async queue design draft.
- [ ] Prepare ScenePanel progress UI placeholders.

## Week 5: Async Resource Loading

### Tasks
1. Implement async texture and model load queue with priorities.
2. Add non-blocking handle API and completion callbacks.
3. Add editor progress UI for pending loads.

### File Targets
- include/ModelLib/Resources/ResourceManager.hpp
- src/ModelLib/Resources/ResourceManager.cpp
- src/Editor/SceneLoader.cpp
- src/Editor/ui/ScenePanel.cpp

### Acceptance Criteria
- Scene import does not block render loop.
- Loading progress visible for large assets.
- Failed loads produce recoverable warnings.

### Validation
1. Import heavy scene and inspect frame-time spikes.
2. Verify callback and cleanup on canceled loads.

## Week 6: Descriptor and Sampler Caching

### Tasks
1. Add material descriptor cache keyed by material state hash.
2. Add sampler cache in device layer.
3. Add cache hit and miss stats.

### File Targets
- include/Engine/Systems/MaterialRenderBindings.hpp
- src/Engine/Systems/MaterialRenderBindings.cpp
- include/Engine/Graphics/Device.hpp
- src/Engine/Graphics/Device.cpp

### Acceptance Criteria
- Descriptor allocations per frame significantly reduced.
- Sampler duplication removed for common sampler modes.
- Cache metrics visible in debug panel.

### Validation
1. Capture frame stats in scene with many materials.
2. Compare descriptor write count before and after.

## Week 7: Render Target Abstraction

### Tasks
1. Create render target wrapper for image, views, sampler and metadata.
2. Move framebuffer-owned image setup into allocator helper.
3. Prepare API for graph-level resource dependency in phase 3.

### File Targets
- include/Engine/Graphics/RenderTarget.hpp
- src/Engine/Graphics/RenderTarget.cpp
- include/Engine/Graphics/FrameBuffer.hpp
- src/Engine/Graphics/FrameBuffer.cpp

### Acceptance Criteria
- Framebuffer code no longer manually manages all target flavors inline.
- Resize path reuses common target build logic.
- HZB and scene-color targets use same abstraction path.

### Validation
1. Run resize and render-path smoke test.
2. Confirm no regression in image layouts and descriptor contents.

## Week 8: Error Boundaries and Fallbacks

### Tasks
1. Define recoverable and fatal error boundary for graphics layer.
2. Add fallback actions for non-fatal resource allocation failures.
3. Add user-facing warning summaries in editor UI.

### File Targets
- include/Engine/Core/Exceptions.hpp
- include/Engine/Core/ErrorCodes.hpp
- src/Engine/Graphics/Buffer.cpp
- src/Engine/Graphics/Descriptors.cpp
- src/Editor/ui/SettingsPanel.cpp

### Acceptance Criteria
- Non-fatal failures degrade quality instead of terminating app.
- Fatal failures include clear diagnostics and actionable reason.
- Error state is visible in editor.

### Validation
1. Inject controlled allocation failure path in test build.
2. Verify fallback quality mode activates.

## End-of-Phase Exit Criteria
- Frame stalls from loading materially reduced.
- Descriptor churn metrics show consistent reduction.
- Editor continues running through recoverable allocation failures.

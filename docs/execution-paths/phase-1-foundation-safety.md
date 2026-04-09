# Phase 1 Execution Path: Foundation and Safety

Timeline: Days 1-30

Status: Done (April 9, 2026)

## Objective
Establish observability, safer runtime switches, and deterministic resize stability.

## Week 1: Structured Logging

### Tasks
1. Add central logger with levels and channels.
2. Replace direct std::cout and std::cerr in render and sync critical paths.
3. Add runtime log filtering in settings UI.

### File Targets
- include/Engine/Core/Logger.hpp
- src/Engine/Core/Logger.cpp
- src/Engine/Graphics/Renderer.cpp
- src/Engine/Graphics/SwapChain.cpp
- src/Engine/Graphics/HZBGenerator.cpp
- src/Editor/ui/SettingsPanel.cpp

### Acceptance Criteria
- Logger supports error, warn, info, debug.
- Renderer and swapchain logs are filterable by channel.
- No direct std::cout usage remains in modified files.

### Validation
1. Build with xmake.
2. Launch Editor and verify channel toggles affect output.

## Week 2: Validation Mode Switches

### Tasks
1. Add xmake options for validation on or off.
2. Add runtime override flag from command line.
3. Document debug and release defaults.

### File Targets
- xmake.lua
- include/Engine/Graphics/Device.hpp
- src/Engine/Graphics/Device.cpp
- readme.md

### Acceptance Criteria
- Debug profile enables validation by default.
- Release profile disables validation by default.
- Runtime override works in both directions.

### Validation
1. Build debug and release variants.
2. Confirm validation messages appear only when enabled.

## Week 3: Swapchain Recreation Coordinator

### Tasks
1. Introduce coordinator class to serialize swapchain recreation phases.
2. Move resize debounce, fence and queue waits into coordinator.
3. Ensure descriptor refresh ordering is deterministic.

### File Targets
- include/Engine/Graphics/SwapChainRecreationCoordinator.hpp
- src/Engine/Graphics/SwapChainRecreationCoordinator.cpp
- src/Engine/Graphics/Renderer.cpp
- src/Engine/Graphics/SwapChain.cpp

### Acceptance Criteria
- All resize recreation flows pass through coordinator.
- No validation errors for in-use semaphore destruction.
- Resize stress loop completes 100 iterations without crash.

### Validation
1. Add resize stress command or manual scripted test.
2. Run with validation enabled and collect zero errors.

## Week 4: Pass-Level GPU Profiling

### Tasks
1. Add timestamp query pool setup.
2. Instrument each pass begin and end.
3. Show per-pass timings in UI panel.

### File Targets
- include/Engine/Graphics/GpuProfiler.hpp
- src/Engine/Graphics/GpuProfiler.cpp
- src/Editor/app.cpp
- src/Engine/Graphics/FrameGraph/RenderGraph.cpp
- src/Editor/ui/SettingsPanel.cpp

### Acceptance Criteria
- Per-pass timing visible in editor.
- Overhead remains low in profiling-disabled mode.
- Profiling data can be exported to JSON.

### Validation
1. Compare frame timing with profiler on and off.
2. Confirm timestamp queries reset correctly each frame.

## End-of-Phase Exit Criteria
- Resize validation errors reduced to zero.
- Stable run for 30 minutes without device-lost.
- Baseline profiling snapshot saved in docs.

## Completion Note
- Phase 1 implementation has been accepted as complete and the project moves to Phase 2.

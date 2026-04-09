# Phase 3 Execution Path: Architecture and Scale

Timeline: Days 61-90

Status: Active (started April 9, 2026)

## Objective
Increase maintainability and scalability by reducing global coupling and automating pass/resource dependencies.

## Week 9: EngineState Decomposition

### Tasks
1. Split EngineState into RenderingState, SceneState, InputState and ResourceState.
2. Introduce lightweight registry for subsystem ownership and initialization ordering.
3. Remove direct cross-subsystem mutable access where avoidable.

### File Targets
- include/Engine/EngineState.hpp
- src/Engine/EngineState.cpp
- include/Engine/SystemRegistry.hpp
- src/Engine/SystemRegistry.cpp
- src/Editor/app.cpp

### Acceptance Criteria
- Systems initialize through registry with explicit dependencies.
- App compiles without direct deep EngineState assumptions.
- Unit tests cover dependency validation.

### Validation
1. Run editor smoke and scene load tests.
2. Confirm subsystem initialization order is deterministic.

## Week 10: Shader Iteration and Variants

### Tasks
1. Add shader hot reload pipeline.
2. Add material variant compilation controls for optional features.
3. Add fallback path for unsupported variant features.

### File Targets
- src/Engine/Graphics/Pipeline.cpp
- src/Engine/Systems/ModelRenderSystem.cpp
- assets/shaders/pbr_shader.frag
- compile_shaders.sh
- compile_shaders.ps1

### Acceptance Criteria
- Shader edits reflect without editor restart.
- Variant selection is deterministic per material features.
- Compilation errors surface cleanly in UI/log.

### Validation
1. Edit shader during runtime and observe update.
2. Validate variant pick with scene containing mixed materials.

## Week 11: Dependency-Aware Render Graph

### Tasks
1. Add resource read/write metadata per pass.
2. Generate transitions and barriers from dependency graph.
3. Detect invalid pass ordering and cyclic dependencies.

### File Targets
- include/Engine/Graphics/FrameGraph/RenderGraph.hpp
- src/Engine/Graphics/FrameGraph/RenderGraph.cpp
- src/Engine/Graphics/Passes/HZBPass.cpp
- src/Engine/Graphics/Passes/OffscreenPass.cpp
- src/Engine/Graphics/Passes/CompositionPass.cpp

### Acceptance Criteria
- Graph emits validated execution order automatically.
- Manual barrier code in pass bodies is reduced.
- Invalid graph setup fails fast with clear messages.

### Validation
1. Run full render pipeline and compare output parity.
2. Run with validation layers and ensure no new sync errors.

## Week 12: Hardening and Release Readiness

### Tasks
1. Add resize stress, descriptor stress and async load stress into test suite.
2. Capture baseline and final performance report.
3. Finalize migration notes and contributor docs.

### File Targets
- tests/Engine/Graphics/
- tests/ModelLib/
- docs/CHANGELOG.md
- readme.md

### Acceptance Criteria
- Stress suites pass reliably.
- Performance metrics show objective improvement.
- Documentation reflects new architecture and workflows.

### Validation
1. Execute full test suite.
2. Run representative benchmark scenes and collect report.

## End-of-Phase Exit Criteria
- Architecture boundaries are explicit and testable.
- Render graph manages dependencies with less manual sync code.
- Team can add new passes and systems with lower regression risk.

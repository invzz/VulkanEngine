# VulkanEngine — Structure & Architecture Improvement Proposal

> Status: refactor IN PROGRESS. Steps 1–9 implemented (step 7 deferred — needs real-GPU runtime verification); README drift (LOW-2) resolved. Source files modified; everything builds and the architecture test suite is green.
> Basis: direct inspection of `src/`, `include/`, `xmake.lua`, `tests/`, and `README.md` on branch `main`.

---

## 0. Progress Log

| Step | Status | What changed |
|------|--------|--------------|
| 1 | **DONE** | Repaired corrupted `OBJImporterTests.cpp` (cube face list was raw C++ text); broke the `ModelLib → Engine` link cycle by injecting the BLAS build as a `std::function<void(Model&)>` callback in `ResourceManager` (removed its `Engine/Graphics/AccelBuilder.hpp` dependency). `xmake run Tests` now builds and runs (636 tests; only GLFW/Vulkan window-creation tests fail — headless env has no display). |
| 2 | **DONE** | Rewrote `ArchitectureDependencyRulesTests.cpp` to enforce the *actual* layered architecture (no more fantasy Clean/Hex ports/adapters). 9 rules, all green. Surfaced 2 real allowlisted violations: `RenderContextAdapter.cpp` (Engine↔Editor glue) and 3 asset-browser panels including `ModelLib/Resources/ResourceManager.hpp`. |
| 3 | **DONE** | Deleted dead `include/Engine/graphics/GraphicsState.hpp` (never instantiated; duplicated `EngineState`'s system ownership + editor flags) and its `#include` in `Engine.hpp`. |
| 4 | **DONE** | Resolved case collision: renamed `Scene/Components/` (capital; held only animation-domain types) → `Scene/Animation/`, both `include/` and `src/` and `tests/`, fixing all `#include` paths. Only `Scene/components/` (ECS) remains. `graphics/` dir was removed in step 3. |
| 5 | **DONE** | Decomposed `EngineState` god object: extracted `TransformService` (`include/Engine/Scene/`, pure domain — get/set T/R/S delegated to the scene registry) and `EnvironmentLightingService` (in `include/Engine/Systems/` because it orchestrates `IBLSystem`/`ProceduralSkyCapture` — runtime concern). The 14 IBL-bake hysteresis fields + sun-light logic + the 12 transform accessors moved out. `DescriptorManager` (already present) now owns all descriptor pools/layouts/per-frame sets; the 16 redundant forwarding accessors (`gbufferDescriptorSet`, `deferredShadow*`, `postProcess*`, `*SetLayout`, `*Pool`, …) were removed from `EngineState`, which now exposes a single `descriptors()` accessor and routes the three render passes through it — keeping `recreatePostProcessingSystem`/`updatePostProcessDescriptors` as composition-layer facades. Public call-site API for non-pass consumers is unchanged. Architecture test caught a misplacement and keeps the descriptor-orchestration guard on `app.cpp`. |
| 6 | **DONE** | Collapsed the dual registries: removed `EngineState::initRegistry_` (the string-keyed `SystemRegistry` used only to topologically order init lambdas). Init order is now a single explicit, dependency-ordered sequence in `initialize()` (core → desc → per-frame descriptors → pipelines/post-processing/input/physics). Runtime lookup remains the `systems_` type_index map (`system<T>()`). The reusable `SystemRegistry` DAG utility + its unit tests stay (self-contained, not dead). |
| 7 | **DEFERRED** | Narrow pass deps via `FrameContext`. The 3 passes still taking `EngineState&` (GbufferPass, DeferredLightingPass, PostProcessPass) use it only for stable accessors (descriptor sets, `scene()`, `renderContext()`, `editor()`, `skybox()`/`skySettings()`). A real `FrameContext` refactor touches every pass ctor + the new `buildDefaultGraph` and cannot be runtime-verified in this headless (no-GPU) environment, so it is intentionally left for a pass when the renderer can be exercised on real hardware. |
| 8 | **DONE** | `RenderPipeline` now owns the default graph: added `RenderPipeline::buildDefaultGraph(EngineState&, Renderer&, Device&, float rtShadowSoftness, UIRenderFn, Window&)` which constructs the full pass chain (update → compute → shadow → depth → gbuffer → deferred → forward → selection mask → transition → post-process → selection composite → composition/ImGui). `App::setupRenderGraph()` in `app.cpp` is now a one-line call. UI rendering stays in the Editor (passed as a callback), so `RenderPipeline` stays decoupled from `ImGuiManager`. |
| 9 | **DONE** | Retired aggregator grab-bag includes. Discovered `Internal.hpp` and `Engine.hpp` are **orphaned** — zero TUs include either, and neither is referenced by `xmake.lua`. Both deleted. `EngineState.hpp` already includes exactly the headers its members need, so no TU lost access. The reusable `SystemRegistry` DAG utility + its unit tests remain (self-contained). |
| 10 | **DONE** | Resolved README drift (finding [LOW-2]): `Project Structure` now lists `Scene/components/` (ECS data), `Scene/Animation/` (domain types) and `Skybox.hpp` instead of the duplicated capital `Components/` entries left from the pre-step-4 layout; the Rendering Pipeline pass list now matches the 12-pass `buildDefaultGraph` order (added Selection Mask, Post-Process, Selection Composite). Verified the listed post-fx features (tonemap / SSAO / bloom / exposure) exist in `PostProcessingSystem` before documenting them. |

---

## 1. Current Scale & Shape

Excluding `third_party` (≈39.7k LOC total incl. vendored):

| Module        | Files | LOC    | Role                                   |
|---------------|-------|--------|----------------------------------------|
| Engine        | 81    | 15,631 | Vulkan backend + ECS systems           |
| Editor        | 41    | 6,183  | ImGui application / panels             |
| ModelLib      | 9     | 2,935  | glTF/OBJ/texture loaders               |
| EngineSceneIO | 1     | 743    | JSON scene serialization               |
| tools         | 2     | 87     | IBLBaker / SceneExporter               |

- 170 public headers, 134 source files.
- Build: `xmake`, C++20; dependencies pinned via `xmake-requires.lock`
  (glfw, glm, imgui-docking, vulkan, entt, joltphysics, tinygltf,
  meshoptimizer, nlohmann_json, gtest).
- Layers as they actually exist:
  - **Domain** — `Scene` + `Components` (EnTT ECS data).
  - **Runtime** — `Engine/Systems/*` + `Engine/Graphics/*` (Vulkan + per-system logic).
  - **Composition** — `EngineState` (god object) + `SystemRegistry` (init ordering).
  - **Application** — `Editor` (ImGui) composes `EngineState` through a thin `app.cpp`.
  - **IO** — `EngineSceneIO`, `ModelLib`, `tools/`.

The underlying direction (ECS + RenderGraph + explicit constructor DI) is sound.
The main structural debt is **left-over architecture from an abandoned
Clean/Hexagonal migration**, plus a non-compiling test target that hides it.

---

## 2. Key Findings

### [HIGH-1] The test suite does not build
`tests/ModelLib/importers/OBJImporterTests.cpp` is corrupted — the cube-face
list was mangled into raw C++ text (lines 44+, e.g. `file << "f 1` with a
newline inside the string literal). `xmake run Tests` aborts at compile, so
**zero tests currently execute on `main`**. The entire regression net
(40+ contract + unit tests) is dead.

### [HIGH-2] Architecture contract test enforces a removed architecture
`tests/Engine/ArchitectureDependencyRulesTests.cpp` asserts a Clean/Hex
design that no longer exists:
- It scans `include/Engine/Application`, `src/Engine/Application`,
  `include/Editor/Infrastructure`, `src/Editor/Infrastructure`,
  `include/Engine/State`, `src/Engine/State` — **none of these directories
  exist** (removed in commit `c75cdbb` "remove ports/adapters/use cases,
  introduce DI container"). `findIncludeViolations()` returns *"Missing
  expected directory"*, so those tests always FAIL.
- It forbids accessors the current code **uses** (`getScene(`, `getIBLSystem()`,
  `getShadowSystem()`, …) and **requires** APIs that don't exist
  (`renderingService().view()`, `sceneRuntimeService().view()`,
  `reconcileSceneLoadUseCase->execute(...)`, `syncEnvironmentLightingUseCase->
  execute(...)`, `ISceneAccessPort`, `IEnvironmentLightingPort`).
- It targets pass headers (`OffscreenPass.hpp`, …) that don't exist, making
  that assertion a silent no-op.

The file is stale and self-contradictory; it cannot pass even if it compiled
and misleads readers into thinking a ports/adapters/use-case layer exists.

### [HIGH-3] Dead `GraphicsState` — split-brain state object
`include/Engine/graphics/GraphicsState.hpp` is **never instantiated** (only
the class definition + one `#include` in `Engine.hpp`). It duplicates
`EngineState`'s entire rendering-system ownership (`modelRender`, `shadow`,
`light`, `skybox`, `grid`, `deferred`, `pp`, `ibl`, `physics`, `jolt`,
`anim`, `collider`, `morph`) **and** re-declares the editor-state toggles
(`showSkybox_`, `showGrid_`, `showDebug_`, `physicsRunning_`, `solidGround_`)
that `EngineState::editor_` already holds. Two parallel "state" objects, one
orphaned. Its `registerXxxSystem(SystemRegistry&)` methods are also never called.

### [HIGH-4] Case-collision directories (latent cross-platform break)
- `include/Engine/Scene/Components` (capital) → `AnimationClip/Controller/Graph`
- `include/Engine/Scene/components` (lowercase) → ECS components (`Transform`, `Camera`, `Light`, …)
- `include/Engine/Graphics` (capital) vs `include/Engine/graphics` (lowercase, holds only the dead `GraphicsState.hpp`)

Source mixes both casings for the same logical area. Fine on Linux
(case-sensitive) but fails to build on macOS/Windows case-insensitive FS,
and is a clarity landmine.

### [MED-1] `EngineState` god object
`EngineState` (256-line header, 467-line `.cpp`) owns ~25 `unique_ptr` systems,
owns `DescriptorManager` + 9 descriptor-set accessors, implements IBL bake
gating (14 hysteresis fields: `procIblLat_`/`procIblDay_`/`procIblPendingTime_`/…),
transform get/set, scene save/load, sun-light driving, and editor toggles. It
is the "single source of truth" the old design was split to avoid — but it
reconcentrated everything.

### [MED-2] Dual registry mechanisms for the same systems
- `SystemRegistry` (string-keyed, dependency-ordered init) — used **only** to
  order init phases inside `EngineState::initialize`.
- `EngineState::systems_` (`unordered_map<type_index, void*>`) + `system<T>()`
  — used for runtime lookup.
Two vocabularies for one set of systems; init order is also encoded in the
lambda dependency list. Redundant.

### [MED-3] Passes coupled to the god object
Render passes reach into `EngineState` via `system<T>()` (e.g.
`PostProcessPass.cpp`, `app.cpp`). The contract test meant to forbid this
(`RenderPassesShouldNotDependOnEngineState`) points at non-existent pass
headers, so it is inert. Passes take no narrow "frame context".

### [MED-4] `app.cpp` is composition root + frame loop + graph builder (612 LOC)
It hand-builds the entire render-graph pass list, wires DI, and runs the loop.
`RenderPipeline` is a 3-method shell that just stores the graph. The
hand-built pass list duplicates the README's pass order and is a long
procedural sequence; a typo silently changes the pipeline.

### [LOW-1] Aggregator header abuse
`Internal.hpp` includes ~70 headers; `Engine.hpp` pulls in the dead
`GraphicsState`. Catch-all includes slow incremental builds and hide real
dependencies.

### [LOW-2] README drift — RESOLVED
Documents Render Graph / ECS / SystemRegistry / DI accurately (good feature
list) but did not reflect that the Clean-Architecture layer was removed, nor
the god-object reality. **Resolved:** `Project Structure` now shows `Scene/components/`
(ECS data) + `Scene/Animation/` (domain types) + `Skybox.hpp` instead of the
old duplicated capital `Components/` entries, and the Rendering Pipeline pass
list now matches the 12-pass `buildDefaultGraph` order (added Selection Mask,
Post-Process, Selection Composite).

---

## 3. Proposed Improvements (suggested order)

### 1) Unblock tests — do first  ✅ DONE
Repair the corrupted face lines in `tests/ModelLib/importers/OBJImporterTests.cpp`
(bad-merge artifact). Then `xmake run Tests` actually runs. ~30 min.

> Done: corrupted cube face list reconstructed (12 triangles / 36 indices);
> the `ModelLib → Engine` link cycle was also broken (see below) so the whole
> `Tests` target links and runs.

### 2) Reconcile the contract test with reality  ✅ DONE
Delete or rewrite `tests/Engine/ArchitectureDependencyRulesTests.cpp`.
Recommended rewrite enforces the **actual, desired** boundaries (not the
removed one):
- Engine headers/sources must not include `Editor/`.
- `Scene/Components` (domain) must not include `Engine/Systems` or `Engine/Graphics`.
- `Systems` must not include `Editor`.
Drop every assertion referencing `Application/`, `Infrastructure/`, `State/`
dirs, use-case ports, and `*Service().view()` APIs — they don't exist. This
restores a *meaningful* architecture guard instead of a failing fantasy.

> Done: file rewritten; 9 rules, all green. It also catches two *real*
> allowlisted violations (Engine↔Editor glue in `RenderContextAdapter.cpp`,
> and 3 asset-browser panels reaching `ModelLib/Resources/ResourceManager.hpp`).

### 3) Kill dead `GraphicsState`
Delete `include/Engine/graphics/` and the `Engine.hpp` `#include`. Pick **one**
state owner (`EngineState`). If you want to shrink it, extract a *used*
`RenderingState` — don't leave two.

### 4) Fix case collisions
- Rename `Scene/Components` → `Scene/Animation` (these are animation-domain
  types, not ECS components) and fix includes. Keep ECS components in
  `Scene/components` — or capitalize everything to `Scene/Components` and move
  animation types out. Choose **one** casing project-wide.
- Remove the lowercase `graphics/` dir entirely (subsumed by step 3).

### 5) Decompose `EngineState` (no port/adapter revival)
Extract narrow, *used* services with explicit ctor DI:
- `DescriptorResources` — the 9 descriptor-set accessors → `DescriptorManager`.
- `EnvironmentLightingService` — IBL bake gating + `syncEnvironmentLighting` + sun.
- `TransformService` — get/set translation/rotation/scale.
`EngineState` keeps the systems registry + the extracted services. Cuts ~150
lines of accessors and the 14 hysteresis fields out of the god object without
reintroducing the removed abstraction layer.

### 6) Collapse the two registries
Keep `system<T>()` (type_index) for runtime lookup. Drive init order from the
explicit construction sequence already in `initCoreSystems`, or one ordered
vector — delete `SystemRegistry`'s string-keyed vocabulary. One mechanism, one
name per system.

### 7) Narrow pass dependencies
Passes take a `FrameContext` (frameIndex, scene view, per-pass descriptor sets)
instead of `EngineState&`. Decouples passes from the god object and makes
`RenderPassesShouldNotDependOnEngineState` enforceable for real.

### 8) Make `RenderPipeline` own the default graph
Move the hand-built pass list out of `app.cpp` into
`RenderPipeline::buildDefaultGraph(EngineState&)`. `app.cpp` becomes just
composition root + window/loop. Split `app.cpp` into `EditorApp` (lifecycle)
and `FrameLoop` if it stays large.

### 9) Retire aggregator grab-bags
Keep `pch.hpp` for the precompiled TU set; stop `Internal.hpp` from being a
global include of ~70 headers. Include specific headers per TU.

---

## 4. Verdict

The engine is well-featured and the core layering (ECS + RenderGraph + ctor DI)
is the right shape. The real problem is **not missing architecture — it is
left-over architecture**: a dead `GraphicsState`, a contract test enforcing an
abandoned Clean/Hex design, a non-compiling test target (so nothing is
verified), and a god-object state class that accumulated everything the old
design was split to avoid.

Fixing **steps 1–4** alone removes the most damaging confusion and restores a
working regression net; **steps 5–9** are the incremental pay-down of the god
object.

### Suggested sequencing
1. Steps 1 + 2 → green test run (restores verification).
2. Steps 3 + 4 → remove dead state + cross-platform hazard.
3. Steps 5 + 6 → decompose god object + one registry.
4. Steps 7 + 8 + 9 → decouple passes, own the graph, trim includes.

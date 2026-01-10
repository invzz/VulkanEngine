# Lightmap Tasks — Sprint Breakdown

This file lists the sprint-by-sprint tasks for the Lightmap Baking project. Each sprint contains goals, concrete subtasks, acceptance criteria, target files and estimates. Use this as the canonical implementation checklist and update owners/estimates as you go.

---

## Sprint 1 — Scene export & importer (Completed) 🔧 ✅
- Goal: Produce an authoritative `scene.json` (authoring) and a generated `scene_lightmaps.json` (bake bindings). Implement a `SceneExporter` and `SceneLoader` + runtime ingestion.
- Estimated effort: 3–7 days
- Deliverables:
  - `docs/scene_schema.md` (json schema)
  - `src/tools/SceneExporter/*` prototype CLI
  - `assets/scenes/demo_scene.json` and `scene_lightmaps.json` (example)
  - `src/Engine/Scene/*` (loader) and `ResourceManager` manifest handling
  - Unit tests for export/import roundtrip
- Subtasks:
  1. Draft `scene.json` authoring schema (assets, objects, lights) and write `docs/scene_schema.md`.
  2. Define **generated** `scene_lightmaps.json` (lightmapBindings map) and schema.
  3. Implement `SceneExporter` CLI that can export current scene from editor or tool into `scene.json`.
  4. Create `SceneLoader` runtime parsing (read `scene.json`) and a `LightmapManifest` loader (reads `scene_lightmaps.json`).
  5. Wire `ResourceManager` to register per-instance lightmap bindings (uvScale/uvOffset, lightmapId).
  6. Add unit tests + a small demo scene that roundtrips authoring → bake manifest → loader.
- Acceptance criteria:
  - `scene.json` and `scene_lightmaps.json` follow schema; loader instantiates objects and assigns per-instance mapping to materials.
  - Exporter produces example scene `assets/scenes/demo_scene.json` and the generated manifest `scene_lightmaps.json`.
- Files:
  - `planning/Lightmap_Baker_Plan.md`, `planning/lightmap_tasks.md`
  - `docs/scene_schema.md`, `src/tools/SceneExporter/*`, `src/Engine/Scene/*`, `src/Engine/Resources/ResourceManager.cpp`

---

## Sprint 2 — UV1 generation & instancing ✳️ (Completed) ✅
- Status: Completed 2026-01-09
- Summary: Integrated `xatlas`, implemented the `UVUnwrap` API, added `UVUnwrapCLI` with an integration test, and added a CI smoke-check that builds and verifies the CLI.
- Goal: Generate unique per-instance UV1 (lightmap UVs) without changing GLTF files. Integrate `xatlas` and provide packing utilities.
- Estimated effort: 1–2 weeks
- Deliverables:
  - `third_party/xatlas` integration + build target
  - `src/tools/UVUnwrap/*` utilities (per-instance mesh cloning, chart generation, atlas packing)
  - `tools/UVUnwrapCLI` binary and integration test
  - Tests and validation tools (overlap checks, padding verification)
- Subtasks:
  1. Add `xatlas` to project (submodule or vendored third_party), add `xmake.lua` target and build rules.
  2. Implement `UVUnwrap::generateInstanceUVs(model, instanceTransform, paddingPx, resolution)` API.
  3. Produce per-instance `uvScale` / `uvOffset` and write results into `scene_lightmaps.json` mapping entries.
  4. Add a CLI tool or library entry to sample/test packing results and ensure no overlaps.
- Acceptance criteria:
  - All instances have non-overlapping UV1 charts for lightmaps, adhere to requested padding, and generated uvScale/uvOffset values fit atlas space.
- Files: `xmake.lua`, `third_party/xatlas`, `src/tools/UVUnwrap/*`

---

## Sprint 3 — Dilation & Mip Generation (MANDATORY) 🧽 (Completed) ✅
- Completed: 2026-01-09
- Summary: Implemented the per-texel `BakeTexel` mask, CPU seam-aware dilation, CPU mip generation that respects validity, added unit & integration tests, and docs. Also added defensive guards to avoid UB under optimization and fixed tests.
- Goal: Implement mandatory validity mask, seam-aware dilation and correct mip generation (average only valid texels).
- Estimated effort: 1–2 weeks
- Deliverables:
  - Bake buffers use `BakeTexel { float3 radiance; uint8_t valid; }`.
  - Dilation implementations (CPU and optional GPU compute shader)
  - Mipgen utilities that respect validity masks and propagate invalid flags
  - Tests that detect halos and energy loss
- Subtasks:
  1. Modify baker outputs to include validity mask per texel; update internal data structures. (Done)
  2. Implement *seam-aware* dilation (preferred): grow valid texels into padding regions using barycentric adjacency or masked dilation passes. (Done)
  3. Add CPU mipgen that computes each mip by averaging only valid contributors. If all invalid → mark invalid. (Done)
  4. Optionally add a GPU mipgen path (blit + compute) for speed; result must match CPU reference tests.
  5. Add automated tests that compare pre/post dilation and ensure no dark halos in mip levels. (Done)
- Acceptance criteria:
  - Dilation fills padding without introducing visible seams; mips preserve energy (verified by tests).
- Files: `src/tools/LightMapBaker/dilate.*`, `src/tools/LightMapBaker/mipgen.*`, test fixtures.

---

## Sprint 4 — VTEX packaging & runtime 📦 (Completed)
- Started: 2026-01-09 (active: 2026-01-10)
- Status: **Completed** — key features implemented and acceptance criteria met; BC6H prototype intentionally deferred; all tests pass locally. Completed: 2026-01-10.
- PR: changes opened and merged (2026-01-10)
- Goal: Add packaging step to produce `.vtex` runtime assets (mips, format conversion) and implement runtime loader with usage semantics.
- Estimated effort: 1–2 weeks (completed)
- Deliverables (status):
  - `--pack-to-vtex` option in `LightMapBaker` CLI — **Done**
  - Format transcode options (`r32f`, `r16f`, `bc6h`) and level/mip packing — **R32F/R16F done; BC6H prototype done (optional, CLI-driven)**
  - `VTexUsage` metadata usage enum and usage field in VTEX header — **Done**
  - `Texture::createFromVTEX()` loader and prefer VTEX in `ResourceManager` — **Done**
- Subtasks (status):
  1. Implement format conversion and image creation/transfer to a VkImage suitable for `ibl_detail::vtex::writeImage`. — **Done (R32F/R16F CPU-only implemented; BC6H written via CLI prototype)**
  2. Extend `VTexIO` header with `VTexUsage` (Generic, IBL, Lightmap) and add code to write/read usage metadata. — **Done**
  3. Wire `LightMapBaker` CLI to write VTEX files with correct metadata and provenance (`createdBy`, `hash`, `paddingPx`, `vkFormat`). — **Done**
  4. Add `Texture::createFromVTEX()` wrapper and prefer `.vtex` in `ResourceManager::loadModel()` / manifest handling paths. — **Done**
  5. Add tests to load written VTEX assets in a headless runtime and verify pixel values vs EXR reference. — **Done (unit + integration tests added; BC6H tests guarded/skipped unless compressor present)**
  6. (Deferred) Add CI smoke job and containerized test step for BC6H (optional; not required for mainline CI per project preference).
  7. (Deferred) Decide final BC6H integration strategy (CLI vs CMP_Core in-process) after evaluating encoder options.
- Acceptance criteria:
  - VTEX files are generated and loadable by runtime; runtime asserts usage-specific invariants for `Lightmap` usage. **Verified locally: tests pass. CI smoke job deferred per project preference.**
- Files: `src/tools/LightMapBaker/*`, `src/Engine/Systems/IBL/VTexIO.*`, `src/Engine/Resources/Texture.*`, `src/Engine/Resources/ResourceManager.cpp`

---

## Sprint 5 — Shader integration & QA 🎯
- Started: 2026-01-10
- Status: **In-Progress** — implementing shader sampling of UV1, per-instance bindings, and demo scenes.
- Goal: Wire baked lightmaps into the renderer and validate shading (multiplicative irradiance by default).
- Estimated effort: 3–6 days
- Deliverables:
  - `material_decode.glsl` updated to sample `uvChannel=1` (UV1) and apply `uvScale/uvOffset`.
  - Per-instance binding or instance buffer to provide uvScale/uvOffset and lightmap texture index.
  - Demo scenes showing baked-only, dynamic-only, and mixed renders.
- Subtasks:
  1. Add material shader code: sample `globalTextures[nonuniformEXT(indices3.z)]` using UV1 and apply multiplicative blending. — **Done**
  2. Ensure `MaterialRenderBindings` or instance buffer exposes uvScale/uvOffset for each instance. — **Not started**
  3. Render test scenes and compare to EXR ground-truth (visual validation). — **Not started**
  4. Add unit/integration render test that compares a small region between baked output and EXR reference. — **Not started**
  5. Create demo scenes (baked-only, dynamic-only, mixed) and record expected results. — **Not started**
- Acceptance criteria:
  - Baked lighting is visible, multiplicative blending behaves correctly, and tests/demos reproduce expected results.
- Files: `assets/shaders/includes/material_decode.glsl`, demo scenes.

---

## Sprint 6 — Tests, CI, docs & polish ✅
- Goal: Add automated tests, CI coverage, documentation, and finalize polish items.
- Estimated effort: 1–2 weeks
- Deliverables:
  - CI job(s): build `LightMapBaker`, run smoke bake on a tiny scene, validate `scene_lightmaps.json` and loadability of `.vtex`.
  - `docs/baking_workflow.md`, CLI reference, manifest schema docs, and migration notes for deprecating `ModelLightBaker`.
  - Performance & QA report (bake times, memory, recommended defaults)
- Subtasks:
  1. Add unit/integration tests to run quick bakes and pixel-compare small regions between EXR reference and loaded VTEX in the runtime.
  2. Add CI job (or expand existing) that runs only on changes to tools or planning files (avoid heavy CI load).
  3. Write docs for authors and integrators (How to bake a scene, VTEX options, manifest formats).
  4. Finalize TODOs: logging, error handling, xmake targets, and update project planning docs.
- Acceptance criteria:
  - CI passes, docs published, and a short QA checklist exists for manual verification.

  ### Golden Image Workflow & Tolerance Rationale ✅
  - Overview: Golden images are authoritative EXR (linear float RGBA) references used to validate renderer sampling of baked lightmaps. Tests compare a small region (e.g., central 8×8) between a rendered output and the golden with both a per-pixel max-difference and RMS threshold.
  - How to update a golden:
    1. Locally run the golden test with the environment flag `UPDATE_GOLDEN=1` to generate the golden EXR into `assets/goldens/` (test will write and skip; re-run without the flag to validate).
    2. Inspect the generated golden visually and verify baking parameters (resolution, padding, formats). If acceptable, commit the golden EXR and include a short note in the commit message describing why it changed.
    3. Prefer small, focused goldens (small resolution or region) to keep CI and storage overhead low.
  - File format & location: store goldens as EXR with linear float RGBA in `assets/goldens/` to ensure deterministic read/write (use `tinyexr` helpers in tests).
  - Tolerance rationale:
    - Use a conservative **max-difference <= 2e-3** and **RMS <= 1e-3** for region comparisons. These values account for half-float rounding (R16), minor shader-to-shader numeric differences, and deterministic post-process conversion (we disable tone mapping/vignette during tests).
    - If tests fail by narrow margins, prefer inspecting the delta image and only update golden after manual verification.
  - CI behaviour: CI runs the golden comparison test; it fails on mismatches. Updating goldens requires an explicit developer step (generate, verify, commit) and should be recorded in the PR description.

- Files: `xmake.lua`, `scripts/ci/*`, `docs/`, `planning/Lightmap_Baker_Plan.md`

---

## Cross-sprint Rules & Mandates (non-negotiable)
- Use a per-texel validity mask (`BakeTexel { float3 radiance; uint8_t valid }`) for dilation and mipgen.
- Mip generation MUST average only valid texels; invalid-dominant destinations must be marked invalid.
- `scene.json` (authoring) must be separate from `scene_lightmaps.json` (generated bindings); do not write bake output into authoring files.
- VTEX must carry `VTexUsage` semantics and runtime assertions for Lightmap usage (linear, mipmaps required).
- Initial shader blending rule: multiplicative (`finalColor.rgb *= bakedLight`).

---

## Definition of Done
- All sprint acceptance criteria satisfied, tests added, CI green, documentation updated and sample scenes added.
- Backwards-compatible runtime behavior (EXR fallback) maintained.

---

Planned next step: Start **Sprint 5** — shader integration & QA (implement material shader changes, per-instance bindings, and demo scenes). If you'd prefer I pivot back to another sprint instead, say so and I'll adjust priorities.
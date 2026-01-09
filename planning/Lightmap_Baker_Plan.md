# Lightmap Baker Implementation Plan

## Summary ✅
- This document captures the concrete implementation plan for adding scene-level lightmap baking to the engine.
- It contains findings, prioritized tasks, deliverables, target files, and a recommended next step.

---

## Findings (short) 🔎
- A functional per-model baker exists: `src/tools/ModelLightBaker/*` (CPU + optional GPU). ✅
- Runtime can assemble per-model mesh atlases from per-mesh manifests and assign `PBRMaterial::lightmap` in `ResourceManager.cpp`. ✅
- Material uniform supports a lightmap index (`indices3.z`) and binding is handled in `MaterialRenderBindings.cpp`. ✅
- Missing or partial items: scene-level `Scene.json` export/import, per-instance `UV1` generation (xatlas recommended), scene-level `LightMapBaker` tool, and shader sampling of baked lightmaps. ⚠️

---

## Concrete Tasks (numbered, with subtasks & files) 🔧

1) **Design & implement `Scene.json` schema & exporter** (in-progress)
   - Deliverables: `docs/scene_schema.md`, `planning/demo_scene.json`, `src/tools/SceneExporter/*` or extend `EngineSceneIO`.
   - Subtasks:
     - Author authoritative schema (assets, objects, lights, lightmaps, per-instance mapping).
     - Export instance transforms, `mobility` flags, GLTF references, and bake metadata.
     - Add unit test + example scene in `assets/scenes/`.
   - Files: `planning/Lightmap_Baker_Plan.md`, `src/tools/SceneExporter/*`, `include/Engine/Scene/*`.

2) **Add Scene importer (runtime) & manifest handling**
   - Deliverables: runtime `SceneLoader` that instantiates objects and stores per-instance lightmap metadata.
   - Subtasks:
     - Parse `Scene.json` and register assets with `ResourceManager`.
     - Persist per-instance mapping (lightmap id, `uvScale`, `uvOffset`) to be read by `MaterialRenderBindings`/mesh binding.
   - Files: `src/Engine/Scene/*`, `src/Engine/Resources/ResourceManager.cpp`.

3) **Implement per-instance `UV1` generation (xatlas integration)**
   - Deliverables: `third_party/xatlas` integration and `src/tools/UVUnwrap/*` utilities.
   - Subtasks:
     - Add `xatlas` as dependency and xmake target.
     - Provide utilities to clone mesh topology per instance, compute UV charts, and pack into atlas with configurable padding.
     - Tests to ensure no overlaps and valid UV bounds.
   - Files: `xmake.lua`, `src/tools/UVUnwrap/*`.

4) **Create top-level `LightMapBaker` tool (scene-level)**
   - Deliverables: `src/tools/LightMapBaker/` CLI that consumes `Scene.json` and emits lightmaps + updated `Scene.json` (or `scene_lightmaps.json`).
   - Subtasks:
     - Use `SceneLoader` + per-instance `UV1` generation, build per-instance geometry for bake.
     - Atlas packing across instances, tile generation, tile naming, and tile manifest output.
     - CLI options: resolution, samples, padding, bounces.
   - Files: `src/tools/LightMapBaker/*`, `xmake.lua` target.

5) **EXR output, dilation/mipgen and VTEX packaging**
   - Deliverables: complete post-process pipeline from EXR tiles → dilated + mipped images → `.vtex` packaging, plus runtime VTEX loader.
   - Subtasks:
     - Produce canonical EXR tiles as authoring artifacts (`lightmaps/<scene>/lm_<id>_tile_<n>.exr`).
     - Implement seam-aware dilation/padding (CPU/GPU) before mip generation.
     - Implement high-quality mip generation (GPU blit preferred, CPU fallback).
     - Implement format conversion / transcode options (float16, BC6H, or store float32) and package into `.vtex` using `ibl_detail::vtex::writeImage`.
     - Add `Texture::createFromVTEX(...)` and prefer `.vtex` in `ResourceManager` with `.exr` fallback.
   - Files: `src/tools/LightMapBaker/*`, `src/Engine/Systems/IBL/VTexIO.*`, `src/Engine/Resources/Texture.*`, `src/Engine/Resources/ResourceManager.cpp`.

6) **Replace / deprecate ModelLightBaker in favor of Scene-level workflows**
   - Note: Per the decision to exclude `ModelLightBaker`, refactor or deprecate it and migrate useful code (BVH, sampling, GPU kernels) into the scene-level `LightMapBaker` library.
   - Deliverables:
     - Migrate core baking primitives from `ModelLightBaker` into shared `lib/baker` utilities.
     - Leave `ModelLightBaker` as a small compatibility tool or remove after migration.
   - Files: `src/tools/ModelLightBaker/*` (refactor/migration), `src/tools/LightMapBaker/*` (consume migrated code).

7) **Runtime shader integration to sample baked lightmaps**
   - Deliverables: shader helper to sample `globalTextures[nonuniformEXT(indices3.z)]` using `uv1` and apply (recommended multiplicative irradiance).
   - Subtasks:
     - Add sampling code to `assets/shaders/includes/material_decode.glsl` and tests/shaders to preview results.
     - Ensure `MaterialRenderBindings` pushes uv scale/offset to uniform or per-instance binding and defines `uvChannel` (default 1).
   - Files: `assets/shaders/includes/material_decode.glsl`, shader variants and demos.

8) **Tests, sample scenes and CI integration**
   - Deliverables: sample scenes, baker smoke tests, CI job to build and run the baker on a small scene and verify EXR→VTEX roundtrip.
   - Subtasks:
     - Add `assets/scenes/baker_test_scene.json`, a small GLTF, and a CI smoke test that verifies outputs and loads them in the runtime.
   - Files: `assets/scenes/`, `xmake.lua`, CI config.

9) **Documentation & user guide**
   - Deliverables: `readme.md` updates, `docs/baking_workflow.md`, manifest format docs, and CLI reference.
   - Subtasks:
     - Step-by-step 'How to bake a scene', CLI examples (`--write-exr`, `--pack-to-vtex`, `--vtex-format`), and manifest schema docs.
   - Files: `readme.md`, `docs/`, `planning/`.

10) **Performance & QA roadmap (denoising, GI, incremental rebake)**
   - Deliverables: plans and experiments for denoising and GI.
   - Subtasks:
     - Add denoising experiments, measure bake times, capture memory and perf metrics.
   - Files: `planning/`, `tools/`.

11) **Polish: build targets, error handling, logging**
   - Deliverables: xmake targets for `LightMapBaker`, robust I/O error handling, verbose logs, and manifest fields for provenance (hash, baker version, paddingPx).
   - Subtasks:
     - Add `xmake` targets and CI hooks, ensure consistent EXR and VTEX naming, improve logging.
   - Files: `xmake.lua`, `src/tools/*`.

---

## Recommended Lightmap Pipeline 🔁
- Bake → Radiance Buffer (float32)
- Write EXR (optional, authoring/debug)
- Dilation / Padding (seam-aware dilation; CPU or GPU compute shader)
- Generate Mips (float linear; GPU blit preferred)
- Compress / Transcode (options: R32F, R16F, BC6H)
- Write VTEX (pack mips + metadata into engine VTEX container)
- Runtime Loads VTEX (fast GPU-ready load; EXR fallback)

**Implementation details:**
- Always dilate before mipgen and format conversion to avoid seams.
- Prefer storing canonical EXR for debugging; ship VTEX for runtime performance.
- VTEX header should include `createdBy`, `hash`, `paddingPx`, `vkFormat` metadata.
- Support CLI flags: `--write-exr`, `--pack-to-vtex`, `--vtex-format={bc6h|r16f|r32f}`, `--padding <px>`, `--mipgen={gpu|cpu}`.

**Naming conventions:**
- `lightmaps/<scene>/lm_<id>_tile_<n>.exr`
- `lightmaps/<scene>/lm_<id>_atlas.vtex` or per-tile `lm_<id>_tile_<n>.vtex`

---

## Targeted Refinements (Strongly Recommended — small, high-payoff changes)
These are short, specific rules to avoid common pitfalls and make the pipeline deterministic and robust.

1) Scene.json: split authoring vs baked data (preferred)
- Keep authoring and generated data separate to avoid merge conflicts and accidental rebakes.
- Pattern (Option A, preferred):
  - `scene.json` → authoring (static content authors edit)
  - `scene_lightmaps.json` → generated by baker (contains bindings/tiles)
- Example `scene_lightmaps.json` snippet:
```json
{
  "lightmapBindings": {
    "object_12": {
      "lightmapId": "lm_000",
      "uvScale": [0.25, 0.25],
      "uvOffset": [0.5, 0.0]
    }
  }
}
```
This avoids merge conflicts, accidental rebakes, and source-control churn.

2) Dilation: require an explicit validity mask (MANDATORY)
- Bake buffers must include a validity mask; do not infer valid pixels from luminance.
- Recommended texel layout:
```cpp
struct BakeTexel {
  float3 radiance;
  uint8_t valid; // 0 or 1
};
```
- Benefits: simplifies dilation, fixes mip averaging, and avoids dark halo artifacts.
- This is mandatory: the dilation step must operate on the validity mask, not on color heuristics.

3) Mip generation rule (IMPORTANT / MANDATORY)
- When generating mips, **average only valid texels**.
- If all contributor texels for a destination mip are invalid, mark the destination as invalid.
- Rationale: prevents energy loss and muddy low-mip appearance; preserves correct lighting behavior.

4) VTEX: lock down semantic usage flags early
- Add a small usage enum to VTEX metadata so tools and runtime can assert correct semantics:
```cpp
enum class VTexUsage : uint8_t {
  Generic,
  IBL,
  Lightmap
};
```
- Runtime can assert:
```cpp
if (usage == VTexUsage::Lightmap) {
  assert(isLinear);
  assert(hasMipmaps);
}
```
- Add `usage` to the scene manifest and to VTEX header so misuse is detectable early.

5) Shader integration: multiply, don’t add (start multiplicative)
- Initial shading rule: multiply finalColor by baked irradiance:
```glsl
finalColor.rgb *= bakedLight;
```
- Rationale: additive baked lighting double-counts energy and complicates tone mapping and exposure.
- You can add additive compositing later if a particular effect requires it, but start with multiplicative.

---

## Risk Areas to Watch (Not problems, just watchpoints)
- Atlas growth: too many instances → mitigation: `--max-atlas-size`, automatic tiling.
- Bake time: CPU-only path can be slow → mitigation: keep GPU path optional and measured.
- CI time: baking scenes in CI can be heavy → mitigation: use tiny test scenes for CI job.
- Shader bindings: per-instance data should be pushed via instance buffer, not per-material updates.

**Priority confirmation:** your ordering is correct. Follow: 1 → 2 → 3 → 5 first, then 4 → 7, and defer denoising/GI until UV1, dilation, mipgen, and VTEX are rock solid.

---

---

## Prioritization & Timeline ✅
- Short-term (High priority): 1 → 2 → 3 → 5 (Scene export/import, UV1 generation, dilation/mipgen, VTEX packaging).
- Medium-term: 4 → 7 (Scene-level baker, shader integration).
- Long-term: 8 → 10 → 11 (tests, QA, docs, polish).

---

## Next step (recommended) ▶️
- Implement the `Scene.json` schema and add a minimal `SceneExporter` prototype that emits `assets/scenes/demo_scene.json` and add the VTEX packaging skeleton (`--pack-to-vtex`) to `LightMapBaker`.

---

*Created by GitHub Copilot — use the project TODO list for tracking progress.*

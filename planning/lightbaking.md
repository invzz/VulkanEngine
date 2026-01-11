# Lightbaking Pipeline & Manifest (Design)

This document records the canonical, implementation- and runtime-facing design for the scene-level lightbaking pipeline.
It answers: what we bake, how we store baked geometry and images, and the exact metadata the runtime needs to bind lightmaps to geometry.

---

## High-level pipeline (step-by-step) ✅

1. Export scene (authoring): `scene.json` — authoritative input. Contains objects, transforms, models, lights, bake flags. No UVs or lightmap info.

2. Parse scene and select bake candidates:
   - **Ignore**: cameras, dynamic lights, dynamic objects.
   - **Bake**: objects that have a `modelPath`, are static, and marked affected by baked light.
   - Result: list of bake candidates (object IDs + transforms).

3. For each bakeable object: collect mesh *instances*:
   - Load model (GLTF), traverse nodes, extract primitives.
   - Apply object + node transforms to produce per-instance geometry.
   - The unit of bake is a MeshInstance (object -> many MeshInstances).

4. Decide resolution per MeshInstance:
   - Compute world-space surface area of the instance.
   - Derive target resolution (e.g., texels per unit area, clamped to min/max).
   - Result: each MeshInstance has Geometry, Transform, TargetResolution.

5. Generate UV1 per MeshInstance (important clarification):
   - **UV1 coordinates MUST be generated and stored in the mesh instance geometry** (e.g., in the builder's mesh/vertex data or in baked per-instance geometry files that the runtime will load).
   - UV1 must be used by the baker for texel→triangle mapping.
   - **Metadata must NOT contain UV coordinate arrays or chart layout.** Metadata only records which UV channel to use (e.g., `uvChannel: 1`).

6. Bake a lightmap per MeshInstance:
   - Allocate an image at the chosen resolution.
   - For each texel: map texel → UV1 → triangle → compute lighting (direct+static indirect/denoising if available) → write texel.
   - Output: one image per MeshInstance.

7. Export VTEX per MeshInstance:
   - Write `*.vtex` file that contains the required channel/format (HDR/encoded) and metadata embedded in the vtex header (format, resolution, padding).

8. Emit scene-level metadata (manifest):
   - The manifest contains bindings only (does not contain UV coordinates).
   - It maps object ids + mesh indices to lightmap paths, `uvChannel` and resolution.

---

## Manifest schema (precise) 🔧

JSON schema (informal):

{
  "version": 1,
  "lightmapBindings": {
    "<objectId>": {
      "meshes": [
        {
          "primitiveIndex": <int>,   // glTF primitive index within the model
          "lightmap": "lightmaps/<scene>/<lm_id>_<file>.vtex",
          "uvChannel": 1,           // ALWAYS indicates which UV set to use (UV1)
          "resolution": [W, H]
        }, ...
      ]
    }, ...
  },
  "lightmaps": [
    {
      "id": "lm_000",
      "file": "lightmaps/<scene>/<lm_id>_<file>.vtex",
      "format": "vtex",
      "resolution": [W,H],
      "usage": "Lightmap"
    }
  ]
}

Notes:
- `lightmapBindings` answers, for each object: for mesh N, which lightmap file to bind and which UV set to sample.
- The manifest is deliberately small: it references files and uvChannel only.
- All paths are relative to project root or scene stem; runtime resolves them as needed.

---

## Naming & layout conventions

- Per-scene output directory: `assets/lightmaps/<scene_stem>/`.
- Per-meshinstance output: `assets/lightmaps/<scene_stem>/<lm_id>_<model_stem>[/<instance>]/<files>`.
- Manifest: `assets/scenes/<scene_stem>_lightmaps.json` (the baker writes it; runtime looks for `<scene>_lightmaps.json` next to `scene.json`).
- uvChannel values: prefer `1` for lightmap UVs (UV1). Runtime must use `uvChannel` to decide sampling.

---

## CLI / tool behavior

- `LightBaker --scene <scene.json> --out <outdir> [--pack-to-vtex] [--auto-uv] [--res <default>]`
  - `--auto-uv`: when present, generate UV1 for each MeshInstance with `UVUnwrap` (xatlas) before baking.
  - `--pack-to-vtex`: after EXR generation, pack to VTEX in-tree and write `.vtex` products.
  - Baker must produce per-meshinstance `*.vtex` and the manifest linking object/meshIndex → vtex.

---

## Runtime requirements (Cube demo + resource manager)

- Scene loader must:
  - Read `scene.json` (objects, models, transforms).
  - Load accompanying `scene_lightmaps.json` (if present) and apply bindings.
  - For each model instance, when creating draw data, assign material/lightmap descriptor(s) per meshIndex using the `lightmap` path and `uvChannel`.
- Mesh assets must provide UV1 in vertex data (either because the original glTF was modified to include UV1, or because we store per-instance baked geometry/overrides for runtime).

  Important: At runtime, UV1 must be indistinguishable from authored UVs — whether achieved by rewriting the glTF, producing derived per-instance meshes, or using per-instance override buffers is an implementation detail and should not change runtime sampling expectations.

---

## Testing & CI ✅

- Add an integration test (hardware-enabled) that:
  - Runs `LightBaker --scene assets/scenes/demo_scene.json --out <tmp> --pack-to-vtex --auto-uv`.
  - Asserts per-meshinstance `.vtex` files exist.
  - Loads `scene_lightmaps.json` and validates: each binding has `uvChannel: 1` and file exists.
  - Launches `Cube` with the `scene.json` and `scene_lightmaps.json` and confirms no runtime errors and the lightmap images are sampled (smoke-check: no missing file or descriptor errors).

- Unit tests for UV generation:
  - `UVUnwrap` returns non-empty UV1 for a small mesh; verify `uv1.size() == vertexCount`.

---

## Implementation roadmap (short-term)

1. Add `--auto-uv` to `LightBaker` and hook into scene bake flow.
2. Ensure `LightmapBakerLib::generateInstanceUVsForNode` is used to populate UV1 into mesh-instance geometry passed to the baker.
3. Bake per-MeshInstance and write `.vtex` files.
4. Write `scene_lightmaps.json` with `primitiveIndex` bindings as specified.
5. Add integration tests and update `scripts/scene_bake.py` to call `--auto-uv` by default when requested.

---

## Important design constraints (summary)

- **UV coordinates are geometry**, not metadata. Do not put UV arrays or chart layout into `scene_lightmaps.json`.
- **Metadata only contains bindings** (file, uvChannel, resolution, optional instance id).
- Keep per-instance granularity: one VTEX per MeshInstance.

---

If you want, I can now implement steps 1–4 (add auto-UV to LightBaker, integrate UV generation into the scene workflow, emit the new manifest and add tests).
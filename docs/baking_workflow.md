# Baking Workflow

This document describes the high-level workflow for generating lightmaps and packaging them into runtime assets.

## Overview

Baking a scene or model follows these main steps:

1. Generate or export a scene description (e.g., `scene.json`) and a bake manifest (`scene_lightmaps.json`).
2. Generate UV1 (lightmap UVs) for instances using `UVUnwrap` / `xatlas`.
3. Run the baker to produce high-quality EXR lightmaps and associated manifests.
4. (Optional) Package EXR into `.vtex` runtime assets for faster load and GPU-ready data.

## Tools

- `UVUnwrapCLI` — generate per-instance UV1 and atlas mappings.
- `ModelLightBaker` — quick per-model baker used for testing and small bakes.
  - Full CLI reference: `docs/ModelLightBaker_CLI.md`

## Packaging & CI

- Use `--pack-to-vtex` or create the sentinel `MODEL_LIGHT_BAKER_PACK_TO_VTEX` in the output folder to request packaging.
- CI includes a smoke job that builds `ModelLightBaker` and validates that a `.vtex` is produced and loadable by the runtime (`Texture::createFromVTEX(..., true)`). See `.github/workflows/lightmap-ci.yml`.

## Troubleshooting & Notes

- If packaging fails: check for a `<name>.pack_attempt` sentinel and inspect logs for the VTEX writer failure.
- Keep bake resolutions small for CI smoke checks (e.g., `--res 16`) to keep runs fast.


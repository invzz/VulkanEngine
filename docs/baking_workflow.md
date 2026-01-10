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

### CI smoke job status & how-to

- Job: `model-light-baker-smoke` (added 2026-01-10) — runs on the self-hosted GPU runner and executes the test `ModelLightBaker.PackToVTEX_CLIProducesVTEX`.
- Where to inspect runs:
  - GitHub UI: Actions → *Lightmap CI* → select the run and the `model-light-baker-smoke` job.
  - GH CLI examples:
    - List recent runs: `gh run list -R invzz/VulkanEngine --workflow=lightmap-ci.yml`
    - View a run: `gh run view <run-id> -R invzz/VulkanEngine --log`
    - Download artifacts: `gh run download <run-id> -R invzz/VulkanEngine --name model-light-baker-logs`
- Artifacts & logs:
  - The job uploads `model-light-baker-logs` (test logs and any generated artifacts like the `.vtex` if saved by tests).
  - Look for `*.log` and the `.vtex` file named `<base>_lightmap.vtex` in the artifact bundle.
- Notes:
  - The job sets `RUN_HARDWARE_TESTS=1` when running the test so hardware-dependent code paths are enabled.
  - If packaging fails, check for a `<name>.pack_attempt` sentinel produced by the tool and inspect the logs for VTEX writer errors.

## Troubleshooting & Notes

- If packaging fails: check for a `<name>.pack_attempt` sentinel and inspect logs for the VTEX writer failure.
- Keep bake resolutions small for CI smoke checks (e.g., `--res 16`) to keep runs fast.


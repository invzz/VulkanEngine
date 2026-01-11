# LightBaker CLI Reference

This document summarizes the command-line usage and flags for the new `LightBaker` tool that replaces `ModelLightBaker` for both per-model and scene-level baking.

## Usage

```text
LightBaker --model <model_path> --out <outdir> [options]
LightBaker --scene <scene.json> --out <outdir> [options]
```

- `--model <model_path>`: Bake a single model file (e.g., `assets/models/glTF/Sponza/glTF/Sponza.gltf`).
- `--scene <scene.json>`: Bake an entire scene described in JSON (scene ingestion).
- `--out <outdir>`: Output directory where EXR / manifests will be written.

## Options

- `--resolution <int>`
  - Force bake resolution (use small values like `16` for CI smoke tests).
- `--sun-intensity <float>`
  - Sun intensity (default: `1.0`).
- `--model` / `--scene` may be combined with `--instance <id>` to bake a specific instance when supported by the scene descriptor.
- `--help`, `-h`
  - Show help and exit.

## Output Files

- `<base>_lightmap.exr` — The high-precision EXR output of the bake.
- `<base>_lightmap.json` — Manifest describing the generated lightmap and metadata. The manifest includes fields such as `bakedLights` and `instanceId` when relevant.

## Examples

```bash
# Bake a single model
LightBaker --model assets/models/glTF/Sponza/glTF/Sponza.gltf --out assets/lightmaps/sponza --resolution 16

# Bake a scene and write manifests
LightBaker --model assets/models/glTF/Sponza/glTF/Sponza.gltf --out assets/lightmaps/sponza --resolution 16 --scene assets/scenes/demo_scene_bake.json
```

## Notes for CI and Tests

- Integration tests should call the `LightBaker` binary (build macro `LIGHT_BAKER_PATH`) and validate generated manifests (e.g., that `bakedLights` contains expected entries).
- The CI smoke job was updated to run `LightBaker` scene tests instead of `ModelLightBaker`.
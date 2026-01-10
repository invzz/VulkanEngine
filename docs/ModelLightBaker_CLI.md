# ModelLightBaker CLI Reference

This document summarizes the command-line usage, flags, and sentinel behavior for the `ModelLightBaker` tool used to bake per-model lightmaps and optionally package them into `.vtex` runtime containers.

## Usage

```text
ModelLightBaker <modelFile> <outputDir> [options]
```

- `modelFile`: Path to a model (e.g., `assets/models/glTF/Sponza/glTF/Sponza.gltf`).
- `outputDir`: Directory where EXR / VTEX / manifests will be written.

## Options

- `--res <px>`
  - Bake resolution (default: 512).
- `--samples <n>`
  - Number of samples for soft penumbra (default: 16).
- `--sun-dir x y z`
  - Sun direction vector (default: `0 -1 0`).
- `--sun-intensity <f>`
  - Sun intensity (default: `1.0`).
- `--mode <texel|vertex|mesh>`
  - Bake granularity (default: `texel`).
- `--epsilon <exp>`
  - Ray epsilon exponent (e.g. `-6` means `1e-6`).
- `--padding <f>`
  - BVH padding fraction of scene extent (default: `0.02`).
- `--pack-to-vtex`
  - Package the produced EXR into a `.vtex` runtime container (output is `<base>_lightmap.vtex`).
  - Equivalent triggers: creating a sentinel file or setting env var (see below).
- `--gpu`
  - Use GPU for baking (default: CPU).
- `--preview [px]`
  - Write a preview PNG optionally clamped to a maximum size.
- `--help`, `-h`
  - Show help and exit.

## Sentinel & Environment Behavior

- Sentinel file: If a file named `MODEL_LIGHT_BAKER_PACK_TO_VTEX` exists in the output directory, the tool will attempt to pack the produced EXR into a `.vtex`.
- Environment variable: Setting `MODEL_LIGHT_BAKER_PACK_TO_VTEX=1` in the calling environment will force packing even if `--pack-to-vtex` is not present on the command line.
- Pack attempt sentinel: When the pack path is entered the tool writes a small attempt sentinel `<name>.pack_attempt` next to the candidate `.vtex` file to indicate the pack path was executed.

## Output Files

- `<base>_lightmap.exr` — The high-precision EXR output of the bake.
- `<base>_lightmap.json` — Per-model manifest describing the generated lightmap and metadata.
- `<base>_lightmap.vtex` — (Optional) VTEX runtime container produced when packing is requested and successful.
- `<base>_lightmap.vtex.pack_attempt` — Diagnostic sentinel indicating a pack attempt occurred.

## Examples

```bash
# Bake Sponza to a small lightmap and package it into a VTEX
ModelLightBaker assets/models/glTF/Sponza/glTF/Sponza.gltf assets/lightmaps/sponza --res 16 --pack-to-vtex

# Same effect triggered by the env var
export MODEL_LIGHT_BAKER_PACK_TO_VTEX=1
ModelLightBaker assets/models/glTF/Sponza/glTF/Sponza.gltf assets/lightmaps/sponza --res 16
```

## Notes for CI and Tests

- Integration tests may create the sentinel file or set the environment variable to deterministically trigger packing.
- The CI smoke job runs `ModelLightBaker` via the test runner and validates that the `.vtex` file exists, has non-zero size, and that `Texture::createFromVTEX(...)` can create a CPU-only texture from the file.

## Troubleshooting

- If the `.vtex` is not written but a `.pack_attempt` exists, inspect the tool logs for failures in the VTEX writer or filesystem permission issues.
- Use `--help` to get a summary of flags and usage.

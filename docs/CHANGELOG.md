# Changelog

## 2026-01-11
- Deprecate `ModelLightBaker` tests and CI smoke job in favor of the new `LightBaker` tool. ✅
- Added `LightBaker` scene integration test (`LightBaker.SceneLights_CLIIncludesBakedLightsInManifest`).
- Removed pack-oriented `ModelLightBaker` test used for VTEX packing (deprecated).
- Updated CI (`.github/workflows/lightmap-ci.yml`) to build `LightBaker` and run the LightBaker scene smoke test on the hardware runner.
- Updated documentation: `docs/baking_workflow.md` and `docs/ModelLightBaker_CLI.md` (now `LightBaker` CLI Reference).

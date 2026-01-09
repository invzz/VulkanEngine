Lightmap CI - Self-hosted GPU Runner Guide

This repository includes a split CI workflow: unit tests run on GitHub-hosted runners (fast & deterministic), and optional hardware-dependent integration tests run on a self-hosted GPU runner.

Overview
- Unit tests: run on `ubuntu-latest` (no GPU required). Hardware tests are skipped by default to keep runs deterministic.
- GPU integration job: a separate job that runs on a self-hosted runner with labels `[self-hosted, linux, gpu]`. The job sets `RUN_HARDWARE_TESTS=1` to enable the hardware tests.

Self-hosted runner requirements
- OS: Linux (Ubuntu recommended).
- GPU drivers: Install NVIDIA/AMD drivers appropriate for your hardware.
- Vulkan: Install Vulkan loader & `vulkan-tools` (provides `vulkaninfo`). Example on Ubuntu:
  - sudo apt-get update
  - sudo apt-get install -y libvulkan1 vulkan-tools
- X.org libs: depending on driver & distro (common packages: libx11-xcb, libxcb, libxrandr, etc.)

Runner setup checklist
1. Install and configure runner per GitHub's self-hosted runner docs.
2. Ensure the runner has access to GPU (drivers installed, `nvidia-smi` available for NVIDIA, or `vulkaninfo` lists devices).
3. Consider running `vulkaninfo` to validate the environment: `vulkaninfo | head -n 50` should list at least one physical device.
4. Tag the runner with labels: `self-hosted`, `linux`, and `gpu` so the CI workflow picks it up.

CI smoke-check script
- The workflow includes a smoke-check step that runs `.github/scripts/check_vulkan.sh` to verify `vulkaninfo` is present and reports a quick device enumeration. This prevents the GPU integration job from running tests on misconfigured runners.

Environment variables
- RUN_HARDWARE_TESTS: if set in the runner environment, tests marked as hardware-dependent will execute; otherwise they skip.
- EXR2VTEX_PATH: optional override to the `EXR2VTEX` tool path (used by tests if present).

Troubleshooting
- If `vulkaninfo` is missing, install `vulkan-tools` from your distro or Vulkan SDK.
- If `vulkaninfo` reports `VK_ERROR_DEVICE_LOST` or no devices, verify driver versions and ensure the kernel module is loaded (e.g., NVIDIA's driver).

Contact
If you need assistance provisioning a runner or interpreting `vulkaninfo` output, add a note to the repo issue tracker or ping the graphics team.
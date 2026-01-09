#!/usr/bin/env bash
set -euo pipefail

echo "[smoke-check] checking for vulkaninfo..."
if command -v vulkaninfo >/dev/null 2>&1; then
  echo "[smoke-check] running 'vulkaninfo' (truncated)"
  vulkaninfo | sed -n '1,80p'
  echo "[smoke-check] vulkaninfo succeeded"
  exit 0
else
  echo "[smoke-check] vulkaninfo not found. Please install vulkan-tools or the Vulkan SDK."
  exit 2
fi

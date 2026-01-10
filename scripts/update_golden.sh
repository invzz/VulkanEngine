#!/usr/bin/env bash
set -euo pipefail

# Helper to (re)generate a golden EXR for a named test.
# Usage: ./scripts/update_golden.sh [TEST_NAME]
# Default test: LightmapGolden.FullscreenRegionCompare

TEST_BIN=build/linux/x86_64/debug/Tests
TEST_NAME=${1:-LightmapGolden.FullscreenRegionCompare}

echo "[update_golden] Building tests..."
xmake -v --all

if [ ! -x "${TEST_BIN}" ]; then
  echo "ERROR: test binary not found at ${TEST_BIN}" 1>&2
  exit 2
fi

echo "[update_golden] Running ${TEST_NAME} (will write golden if UPDATE_GOLDEN=1)..."
# Set UPDATE_GOLDEN to 1 to force golden write. The test will write and skip itself.
UPDATE_GOLDEN=1 "${TEST_BIN}" --gtest_filter="${TEST_NAME}" || true

echo "[update_golden] Done. If a golden was written it will be in assets/goldens/; inspect and commit the file if correct."
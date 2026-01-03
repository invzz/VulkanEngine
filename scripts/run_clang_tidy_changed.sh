#!/usr/bin/env bash
set -euo pipefail

# Run clang-tidy on C/C++ source files changed vs a base ref.
#
# Usage:
#   ./scripts/run_clang_tidy_changed.sh [base-ref]
#
# Notes:
# - Uses compile_commands.json from repo root.

BASE_REF="${1:-}"
JOBS="${JOBS:-1}"
COMPILE_DB_DIR="."

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Escape for regex use in --header-filter
REPO_ROOT_REGEX="$(printf '%s' "$REPO_ROOT" | sed -e 's/[.[\^$*+?()|{}]/\\&/g')"
HEADER_FILTER="^${REPO_ROOT_REGEX}/(include|src)/"

if ! command -v git >/dev/null 2>&1; then
  echo "git is required to run this script." >&2
  exit 1
fi

if [ ! -f "${COMPILE_DB_DIR}/compile_commands.json" ]; then
  echo "compile_commands.json not found in '${COMPILE_DB_DIR}'. Run: xmake project -k compile_commands" >&2
  exit 1
fi

ref_exists() {
  git rev-parse --verify "$1^{commit}" >/dev/null 2>&1
}

if [ -z "$BASE_REF" ]; then
  for c in origin/main main origin/master master; do
    if ref_exists "$c"; then BASE_REF="$c"; break; fi
  done
  if [ -z "$BASE_REF" ]; then
    BASE_REF="HEAD~1"
  fi
fi

if ! ref_exists "$BASE_REF"; then
  echo "Base ref '$BASE_REF' not found. Provide a valid base ref." >&2
  exit 1
fi

mapfile -t files < <(git diff --name-only --diff-filter=ACMRTUXB "$BASE_REF...HEAD" || true)

if [ ${#files[@]} -eq 0 ]; then
  echo "No changed files vs $BASE_REF." >&2
  exit 0
fi

# Filter to translation units only
mapfile -t tus < <(
  printf '%s\n' "${files[@]}" |
    grep -E '^(src|include)/' |
    grep -E '\.(c|cc|cpp|cxx)$' || true
)

if [ ${#tus[@]} -eq 0 ]; then
  echo "No changed C/C++ translation units (.c/.cc/.cpp/.cxx) vs $BASE_REF." >&2
  exit 0
fi

echo "Running clang-tidy on ${#tus[@]} file(s) vs $BASE_REF"

run_one() {
  local f="$1"
  echo "clang-tidy: $f"
  clang-tidy -p "$COMPILE_DB_DIR" -quiet --extra-arg=-w --header-filter="$HEADER_FILTER" "$f"
}

failed=0
if [ "${JOBS}" -gt 1 ] && command -v xargs >/dev/null 2>&1; then
  export -f run_one
  export COMPILE_DB_DIR HEADER_FILTER
  # Note: bash -c receives arguments as $0..; use $1 for the file
  if ! printf '%s\0' "${tus[@]}" | xargs -0 -n 1 -P "${JOBS}" bash -c 'run_one "$1"' _; then
    failed=1
  fi
else
  for f in "${tus[@]}"; do
    if ! run_one "$f"; then
      failed=$((failed+1))
    fi
  done
fi

if [ "$failed" -ne 0 ]; then
  echo "clang-tidy reported issues." >&2
  exit 1
fi

echo "clang-tidy finished successfully."

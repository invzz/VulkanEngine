#!/usr/bin/env python3
"""Scan assets/models/glTF and (re)generate glTF/model-index.json.

Mirrors how SceneUI parses the index (src/Editor/ui/Panels/SceneUI.cpp):
  - Each entry has "name" (the model directory under assets/models/glTF/).
  - Optional "variants.glTF": the actual .gltf/.glb filename inside
    glTF/<name>/glTF/. When absent, the UI falls back to
    glTF/<name>/glTF/<name>.gltf.

Usage:
  python3 scripts/generate_model_index.py [--root <repo>] [--dry-run] [--force]

  --root     Repo root (default: parent of this script's directory).
  --dry-run  Print the resulting JSON without writing the file.
  --force    Recompute variants for every entry (discard previously parsed
             variants). Default behaviour merges: keeps existing entries'
             extra fields and only adds/updates entries for found folders.
"""

import argparse
import json
import os
import sys

MODEL_EXTS = (".gltf", ".glb")


def find_gltf_files(model_dir):
    """Return .gltf/.glb files under model_dir/glTF (the nested glTF folder).

    The SceneUI parser always appends 'glTF/<variant>' after the model dir,
    so we only trust files located inside the nested 'glTF/' subfolder.
    """
    nested = os.path.join(model_dir, "glTF")
    found = []
    if not os.path.isdir(nested):
        # Fall back to scanning the whole model dir one level deep.
        for root, _dirs, files in os.walk(model_dir):
            for f in files:
                if f.lower().endswith(MODEL_EXTS):
                    found.append(os.path.relpath(os.path.join(root, f), model_dir))
        return found
    for root, _dirs, files in os.walk(nested):
        for f in files:
            if f.lower().endswith(MODEL_EXTS):
                found.append(os.path.relpath(os.path.join(root, f), nested))
    return sorted(found)


def build_entry(name, model_dir):
    """Build one index entry for a model directory."""
    files = find_gltf_files(model_dir)
    entry = {"name": name}
    if files:
        # Prefer the file whose stem matches the folder name; else the first.
        chosen = None
        for rel in files:
            base = os.path.basename(rel)
            if os.path.splitext(base)[0].lower() == name.lower():
                chosen = base
                break
        if chosen is None:
            chosen = os.path.basename(files[0])
        entry["variants"] = {"glTF": chosen}
    return entry


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_root = os.path.dirname(script_dir)

    ap = argparse.ArgumentParser(description="Generate glTF/model-index.json")
    ap.add_argument("--root", default=default_root, help="Repo root directory")
    ap.add_argument("--dry-run", action="store_true", help="Print JSON, do not write")
    ap.add_argument("--force", action="store_true",
                    help="Recompute every entry instead of merging")
    args = ap.parse_args()

    gltf_root = os.path.join(args.root, "assets", "models", "glTF")
    if not os.path.isdir(gltf_root):
        print(f"error: glTF model directory not found: {gltf_root}", file=sys.stderr)
        sys.exit(1)

    index_path = os.path.join(gltf_root, "model-index.json")

    # Load existing index to preserve extra fields and ordering.
    existing = {}
    if os.path.isfile(index_path) and not args.force:
        try:
            with open(index_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            for item in data:
                if isinstance(item, dict) and "name" in item:
                    existing[item["name"]] = item
        except (json.JSONDecodeError, OSError) as e:
            print(f"warning: could not read existing index ({e}); starting fresh",
                  file=sys.stderr)

    # Scan model directories.
    new_entries = {}
    for name in sorted(os.listdir(gltf_root)):
        model_dir = os.path.join(gltf_root, name)
        if not os.path.isdir(model_dir):
            continue
        # Skip the index file itself if it ever sits loose in this dir.
        if name == "model-index.json":
            continue
        if not find_gltf_files(model_dir):
            print(f"skip: {name} (no .gltf/.glb found)", file=sys.stderr)
            continue

        built = build_entry(name, model_dir)
        if name in existing:
            # Merge: keep extra fields from the existing entry, but refresh
            # name (identity) and the detected variants.
            merged = dict(existing[name])
            merged["name"] = name
            if "variants" in built:
                merged["variants"] = built["variants"]
            new_entries[name] = merged
        else:
            new_entries[name] = built

    # Preserve order: existing first, then newly discovered.
    ordered_names = []
    for n in existing:
        if n in new_entries:
            ordered_names.append(n)
    for n in sorted(new_entries):
        if n not in ordered_names:
            ordered_names.append(n)

    result = [new_entries[n] for n in ordered_names]

    if args.dry_run:
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return

    with open(index_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {len(result)} entr{'y' if len(result) == 1 else 'ies'}: {index_path}")


if __name__ == "__main__":
    main()

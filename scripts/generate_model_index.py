#!/usr/bin/env python3
"""Scan assets/models and (re)generate assets/models/model-index.json.

Mirrors the loader in src/Editor/ui/Panels/SceneUI.cpp. Every path in the
index is stored RELATIVE TO MODEL_PATH (= assets/models/), so the C++ loader
just concatenates MODEL_PATH + entry["file"] (or entry["variants"][key]).

Collections are the top-level directories under assets/models/:
  - If a collection contains a "Models/" subdirectory, its children are the
    model directories (Khronos sample-models layout).
  - Otherwise the collection's direct children are the model directories
    (Intel layout: assets/models/intel/<model>/...).

Index entry schema:
  {
    "name":    "<folder name / stable id>",
    "display": "<metadata.json name or folder name>",   # optional
    "root":    "<collection subpath relative to MODEL_PATH>",
    "file":    "<relative path to default variant>",
    "variants": { "<variantKey>": "<relative path>", ... }
  }

Variant keys:
  - ".glb" files -> "glTF-Binary" (the default, when present)
  - ".gltf" files inside a "glTF*" subfolder -> "glTF"
  - ".gltf" elsewhere -> the file stem (fallback)
  - other subfolders (e.g. "glTF-Draco") -> their folder name as-is

Default "file" priority: glTF-Binary/*.glb -> glTF/*.gltf -> any *.glb ->
any *.gltf.

Usage:
  python3 scripts/generate_model_index.py [--root <repo>] [--dry-run] [--force]
"""

import argparse
import json
import os
import sys

GLTF_EXTS = (".gltf", ".glb")


def collect_variants(model_dir):
    """Return {variant_key: relative_path} for a model directory.

    relative_path is relative to the model_dir. Keys are chosen so that each
    glTF subfolder stays distinct (we must NOT collapse every .glb into a
    single "glTF-Binary" key, or the KTX/Draco compressed variants would
    overwrite the canonical one):

      - file in subfolder "glTF"              -> "glTF"
      - file in subfolder "glTF-Binary"       -> "glTF-Binary" (canonical default)
      - file in any other subfolder (glTF-Draco, glTF-Binary-KTX-ETC1S-Draco,
        source, ...)                           -> that subfolder's name
      - root-level file with no subfolder      -> its own stem
    """
    variants = {}
    for root, _dirs, files in os.walk(model_dir):
        for f in files:
            lower = f.lower()
            if not lower.endswith(GLTF_EXTS):
                continue
            rel = os.path.relpath(os.path.join(root, f), model_dir)
            parts = rel.split(os.sep)
            sub = parts[0] if len(parts) > 1 else ""
            sub_lower = sub.lower()
            if sub_lower == "gltf":
                key = "glTF"
            elif sub_lower == "gltf-binary":
                key = "glTF-Binary"
            elif sub:
                key = sub  # e.g. "glTF-Draco", "glTF-Binary-KTX-ETC1S-Draco", "source"
            else:
                key = os.path.splitext(f)[0]  # root-level file: use its stem
            # Prefer the first (top-most) occurrence of each key.
            if key not in variants:
                variants[key] = rel
    return variants


def pick_default(variants):
    """Choose the default relative path among detected variants.

    Priority: canonical glTF-Binary subfolder -> glTF subfolder ->
    any (uncompressed) .gltf -> any .glb. We prefer a separated .gltf over a
    root-level .glb because some root-level .glb files are compressed
    (e.g. sponza-4k's AVIF/WebP .glb) and need a decoder the engine may lack.
    """
    for preferred in ("glTF-Binary", "glTF"):
        if preferred in variants:
            return variants[preferred]
    for v in variants.values():
        if v.lower().endswith(".gltf"):
            return v
    for v in variants.values():
        if v.lower().endswith(".glb"):
            return v
    for v in variants.values():
        return v
    return None


def collect_screenshots(model_dir):
    """Return (primary, list) image paths relative to model_dir.

    Resolution order:
      - Khronos: metadata.json["screenshot"] (relative to model_dir) if it
        points to an existing file; otherwise any screenshot/screenshot.* .
      - Intel / fallback: scan the screenshot/ subfolder for images.
    `list` contains every discovered screenshot (relative to model_dir);
    `primary` is the chosen one (or "" if none).
    """
    img_exts = (".png", ".jpg", ".jpeg", ".webp", ".bmp", ".tga")
    is_img = lambda f: f.lower().endswith(img_exts)

    found = []
    shot_dir = os.path.join(model_dir, "screenshot")
    if os.path.isdir(shot_dir):
        for f in sorted(os.listdir(shot_dir)):
            if is_img(f):
                found.append(os.path.join("screenshot", f))

    # Khronos metadata.json hint.
    meta = os.path.join(model_dir, "metadata.json")
    primary = ""
    if os.path.isfile(meta):
        try:
            with open(meta, "r", encoding="utf-8") as mf:
                data = json.load(mf)
            hint = data.get("screenshot")
            if isinstance(hint, str) and hint:
                hint_rel = hint.replace("\\", "/")
                if os.path.isfile(os.path.join(model_dir, hint_rel)):
                    primary = hint_rel
                    if primary not in found:
                        found = [primary] + [x for x in found if x != primary]
        except (json.JSONDecodeError, OSError):
            pass

    if not primary and found:
        primary = found[0]

    return primary, found


def load_display_name(model_dir):
    """Return a friendly display name from metadata.json if present."""
    meta = os.path.join(model_dir, "metadata.json")
    if os.path.isfile(meta):
        try:
            with open(meta, "r", encoding="utf-8") as f:
                data = json.load(f)
            name = data.get("name") or data.get("title")
            if isinstance(name, str) and name.strip():
                return name.strip()
        except (json.JSONDecodeError, OSError):
            pass
    return None


def collect_readme(model_dir):
    """Return the relative (to model_dir) path to a README, or "" if none.

    Checks README.md first, then README.body.md (Khronos), then falls back to
    any README* file. The UI loads the file on demand via MODEL_PATH + readme.
    """
    for name in ("README.md", "README.body.md"):
        if os.path.isfile(os.path.join(model_dir, name)):
            return name
    for f in sorted(os.listdir(model_dir)):
        if f.upper().startswith("README") and f.lower().endswith((".md", ".txt")):
            return f
    return ""


def iter_collections(models_root):
    """Yield (collection_name, collection_path) for each top-level dir."""
    for name in sorted(os.listdir(models_root)):
        path = os.path.join(models_root, name)
        if os.path.isdir(path):
            yield name, path


def model_dirs_in_collection(collection_path):
    """Yield (model_name, model_path) for each model directory."""
    models_sub = os.path.join(collection_path, "Models")
    base = models_sub if os.path.isdir(models_sub) else collection_path
    for name in sorted(os.listdir(base)):
        path = os.path.join(base, name)
        if os.path.isdir(path) and not name.startswith("."):
            yield name, path


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default_root = os.path.dirname(script_dir)

    ap = argparse.ArgumentParser(description="Generate assets/models/model-index.json")
    ap.add_argument("--root", default=default_root, help="Repo root directory")
    ap.add_argument("--dry-run", action="store_true", help="Print JSON, do not write")
    ap.add_argument("--force", action="store_true",
                    help="Recompute every entry instead of merging")
    args = ap.parse_args()

    models_root = os.path.join(args.root, "assets", "models")
    if not os.path.isdir(models_root):
        print(f"error: models directory not found: {models_root}", file=sys.stderr)
        sys.exit(1)

    index_path = os.path.join(models_root, "model-index.json")

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

    new_entries = {}
    for coll_name, coll_path in iter_collections(models_root):
        for model_name, model_path in model_dirs_in_collection(coll_path):
            variants_rel = collect_variants(model_path)
            if not variants_rel:
                print(f"skip: {coll_name}/{model_name} (no .gltf/.glb found)",
                      file=sys.stderr)
                continue

            default_rel = pick_default(variants_rel)
            assert default_rel is not None  # variants_rel is non-empty here
            # model_path's parent is the base dir the loader must anchor paths
            # to (e.g. "khronos/Models" for Khronos, "intel" for Intel).
            root_rel = os.path.relpath(os.path.dirname(model_path), models_root)

            # Build relative paths against MODEL_PATH.
            variants = {
                k: os.path.join(root_rel, model_name, v).replace(os.sep, "/")
                for k, v in variants_rel.items()
            }
            default_file = os.path.join(root_rel, model_name, default_rel).replace(os.sep, "/")

            entry = {
                "name": model_name,
                "root": root_rel.replace(os.sep, "/"),
                "file": default_file,
                "variants": variants,
            }
            display = load_display_name(model_path)
            if display:
                entry["display"] = display

            # Screenshots (relative to MODEL_PATH).
            shot_primary_rel, shot_all_rel = collect_screenshots(model_path)
            if shot_primary_rel:
                entry["screenshot"] = os.path.join(
                    root_rel, model_name, shot_primary_rel).replace(os.sep, "/")
                if len(shot_all_rel) > 1:
                    entry["screenshots"] = [
                        os.path.join(root_rel, model_name, s).replace(os.sep, "/")
                        for s in shot_all_rel
                    ]

            readme_rel = collect_readme(model_path)
            if readme_rel:
                entry["readme"] = os.path.join(
                    root_rel, model_name, readme_rel).replace(os.sep, "/")

            if model_name in existing:
                merged = dict(existing[model_name])
                merged["name"] = model_name
                merged["root"] = entry["root"]
                merged["file"] = entry["file"]
                merged["variants"] = entry["variants"]
                if display:
                    merged["display"] = display
                if "screenshot" in entry:
                    merged["screenshot"] = entry["screenshot"]
                    merged["screenshots"] = entry.get("screenshots", [entry["screenshot"]])
                else:
                    merged.pop("screenshot", None)
                    merged.pop("screenshots", None)
                if "readme" in entry:
                    merged["readme"] = entry["readme"]
                else:
                    merged.pop("readme", None)
                new_entries[model_name] = merged
            else:
                new_entries[model_name] = entry

    # Preserve order: existing first, then newly discovered (sorted).
    ordered = [n for n in existing if n in new_entries]
    for n in sorted(new_entries):
        if n not in ordered:
            ordered.append(n)

    result = [new_entries[n] for n in ordered]

    if args.dry_run:
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return

    with open(index_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote {len(result)} entr{'y' if len(result) == 1 else 'ies'}: {index_path}")


if __name__ == "__main__":
    main()

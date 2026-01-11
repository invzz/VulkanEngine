#!/usr/bin/env python3
"""Simple scene-level baker wrapper.

Usage:
  python3 scripts/scene_bake.py --scene path/to/scene.json [--out-dir assets/lightmaps/<scene>] [--baker /path/to/ModelLightBaker] [--res 512] [--pack-to-vtex] [--dry-run]

Behavior:
- Parses the scene JSON and finds objects with a `modelPath` field.
- Deduplicates model paths and runs ModelLightBaker once per unique model.
- Places per-model outputs under OUT_DIR/<model_stem>/ and references those files in a generated
  scene_lightmaps.json manifest (default written to assets/scenes/<scene_stem>_lightmaps.json).
- Binds each scene object (by its `id`) to the produced lightmap id (lm_000, lm_001...).

This is intentionally minimal (UV mappings are set to default uvChannel=1, uvScale=[1,1], uvOffset=[0,0]).
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def find_baker(bin_arg):
    if bin_arg:
        p = Path(bin_arg)
        if p.exists():
            return str(p)
        else:
            raise FileNotFoundError(f"Provided ModelLightBaker not found: {bin_arg}")

    # Prefer tools/ModelLightBaker (installed copy), fall back to build path
    candidates = [Path("tools/ModelLightBaker"), Path("build/linux/x86_64/debug/ModelLightBaker"), Path("build/linux/x86_64/release/ModelLightBaker")]
    for c in candidates:
        if c.exists():
            return str(c)
    raise FileNotFoundError("ModelLightBaker binary not found; build the project or pass --baker")


def run_baker(baker_bin, model_path, out_dir, scene_file, res, samples, pack, extra_args, dry_run=False):
    cmd = [baker_bin, model_path, str(out_dir), "--res", str(res), "--samples", str(samples)]
    if scene_file:
        cmd += ["--scene", str(scene_file)]
    if pack:
        cmd.append("--pack-to-vtex")
    if extra_args:
        cmd += extra_args

    print("Running:", " ".join(cmd))
    if dry_run:
        return 0
    r = subprocess.run(cmd)
    return r.returncode


def find_uvunwrap(bin_arg):
    if bin_arg:
        p = Path(bin_arg)
        if p.exists():
            return str(p)
        else:
            raise FileNotFoundError(f"Provided UVUnwrapCLI not found: {bin_arg}")

    candidates = [Path("tools/UVUnwrapCLI"), Path("build/linux/x86_64/debug/UVUnwrapCLI"), Path("build/linux/x86_64/release/UVUnwrapCLI")]
    for c in candidates:
        if c.exists():
            return str(c)
    raise FileNotFoundError("UVUnwrapCLI binary not found; build the project or pass --uvunwrap-bin")



def main():
    parser = argparse.ArgumentParser(description="Scene-level baker wrapper: runs ModelLightBaker per-model and emits scene_lightmaps.json")
    parser.add_argument("--scene", required=True, help="Path to scene.json (authoring)")
    parser.add_argument("--out-dir", help="Output base dir under assets/lightmaps (default: assets/lightmaps/<scene_stem>)")
    parser.add_argument("--baker", help="Path to ModelLightBaker binary (optional)")
    parser.add_argument("--res", type=int, default=512, help="Bake resolution to pass to ModelLightBaker")
    parser.add_argument("--samples", type=int, default=16, help="Samples to pass to ModelLightBaker")
    parser.add_argument("--pack-to-vtex", action="store_true", help="Pass --pack-to-vtex to ModelLightBaker so VTEX files are produced")
    parser.add_argument("--dry-run", action="store_true", help="Don't actually invoke external tools; print planned actions")
    parser.add_argument("--manifest-out", help="Write scene_lightmaps.json to this path (default: assets/scenes/<scene_stem>_lightmaps.json)")
    parser.add_argument("--extra", nargs=argparse.REMAINDER, help="Extra args to append to ModelLightBaker invocation")
    parser.add_argument("--auto-uv", action="store_true", help="Automatically run UVUnwrapCLI per-model to compute uvScale/uvOffset for instances")
    parser.add_argument("--uvunwrap-bin", help="Path to UVUnwrapCLI binary (optional)")
    parser.add_argument("--per-instance", action="store_true", help="Run ModelLightBaker once per-scene instance (passes --instance <id> to the baker)")

    args = parser.parse_args()

    scene_file = Path(args.scene)
    if not scene_file.exists():
        print(f"Scene file not found: {scene_file}")
        sys.exit(2)

    baker_bin = find_baker(args.baker)
    print(f"Using ModelLightBaker: {baker_bin}")

    # Default out dir
    scene_stem = scene_file.stem
    default_outdir = Path("assets/lightmaps") / scene_stem
    out_base = Path(args.out_dir) if args.out_dir else default_outdir
    out_base.mkdir(parents=True, exist_ok=True)

    # Parse scene and collect models
    scene = json.loads(scene_file.read_text())
    objects = scene.get("objects", [])

    model_to_instances = {}
    for obj in objects:
        model = None
        # Support both "modelPath" and older "mesh" keys
        if "modelPath" in obj:
            model = obj["modelPath"]
        elif "mesh" in obj:
            model = obj["mesh"]
        if model:
            model_to_instances.setdefault(model, []).append(obj.get("id", obj.get("name", "unknown")))

    if not model_to_instances:
        print("No models found in scene to bake.")
        sys.exit(0)

    # Deduplicate model paths while preserving order
    unique_models = list(model_to_instances.keys())

    # Compose manifest structure
    scene_lm = {
        "version": 1,
        "lightmapBindings": {},
        "lightmaps": []
    }

    # Run baker per unique model (optionally per-instance)
    lm_counter = 0
    for model in unique_models:
        instances = model_to_instances.get(model, [])
        # Ensure model exists; try to resolve relative to project if needed
        model_path = Path(model)
        if not model_path.exists():
            alt = Path("assets") / model
            if alt.exists():
                model_path = alt
            else:
                alt2 = Path("assets/models") / model
                if alt2.exists():
                    model_path = alt2
                else:
                    print(f"Warning: model file not found: {model}; skipping")
                    continue

        model_stem = model_path.stem
        # Base output dir for this model
        model_out_dir = out_base / f"{model_stem}"
        model_out_dir.mkdir(parents=True, exist_ok=True)

        if not args.per_instance:
            lm_id = f"lm_{lm_counter:03d}"
            print(f"Baking model: {model} -> id={lm_id}")

            per_model_out = model_out_dir / f"{lm_id}_{model_stem}"
            per_model_out.mkdir(parents=True, exist_ok=True)

            ret = run_baker(baker_bin, str(model_path), per_model_out, scene_file, args.res, args.samples, args.pack_to_vtex, args.extra or [], dry_run=args.dry_run)
            if ret != 0:
                print(f"ModelLightBaker failed for {model} (code {ret}), skipping")
                continue

            # Find manifest produced for this model
            manifest_candidates = list(per_model_out.glob(f"*lightmap.json"))
            if not manifest_candidates:
                print(f"Warning: manifest not found for {model} in {per_model_out}")
                continue
            manifest_path = manifest_candidates[0]

            m = json.loads(manifest_path.read_text())
            out_file = m.get("file", "")
            fmt = m.get("format", "exr")
            resolution = m.get("resolution", 0)

            out_file_path = per_model_out / out_file
            if not out_file_path.exists():
                print(f"Warning: expected lightmap file not found: {out_file_path}")

            rel_path = Path("lightmaps") / scene_stem / f"{lm_id}_{out_file}"
            scene_lm["lightmaps"].append({
                "id": lm_id,
                "file": str(rel_path).replace('\\', '/'),
                "format": fmt,
                "resolution": resolution if isinstance(resolution, list) or isinstance(resolution, int) else resolution,
                "usage": "Lightmap"
            })

            for iid in instances:
                scene_lm["lightmapBindings"][iid] = {
                    "lightmapId": lm_id,
                    "uvChannel": 1,
                    "uvScale": [1.0, 1.0],
                    "uvOffset": [0.0, 0.0]
                }

            # Copy produced file into assets tree under out_base/<lm_id>_<filename>
            target_dir = out_base
            target_dir.mkdir(parents=True, exist_ok=True)
            try:
                tgt = target_dir / f"{lm_id}_{out_file}"
                if not args.dry_run:
                    if out_file_path.exists():
                        import shutil

                        shutil.copy2(out_file_path, tgt)
                    else:
                        print(f"Note: produced file does not exist (skipping copy): {out_file_path}")
            except Exception as e:
                print(f"Warning: failed to copy produced lightmap: {e}")

            lm_counter += 1

        else:
            # Per-instance mode: run baker once per scene instance
            if not instances:
                print(f"No instances found for model {model}; skipping per-instance bakes")
                continue

            for iid in instances:
                lm_id = f"lm_{lm_counter:03d}"
                print(f"Baking model instance: {model} instance={iid} -> id={lm_id}")

                inst_out_dir = model_out_dir / f"{iid}"
                inst_out_dir.mkdir(parents=True, exist_ok=True)

                extra = list(args.extra or [])
                extra += ["--instance", str(iid)]
                ret = run_baker(baker_bin, str(model_path), inst_out_dir, scene_file, args.res, args.samples, args.pack_to_vtex, extra, dry_run=args.dry_run)
                if ret != 0:
                    print(f"ModelLightBaker failed for {model} instance {iid} (code {ret}), skipping")
                    lm_counter += 1
                    continue

                # Find manifest produced for this instance
                manifest_candidates = list(inst_out_dir.glob(f"*lightmap.json"))
                if not manifest_candidates:
                    print(f"Warning: manifest not found for {model} instance {iid} in {inst_out_dir}")
                    lm_counter += 1
                    continue
                manifest_path = manifest_candidates[0]

                m = json.loads(manifest_path.read_text())
                out_file = m.get("file", "")
                fmt = m.get("format", "exr")
                resolution = m.get("resolution", 0)

                out_file_path = inst_out_dir / out_file
                if not out_file_path.exists():
                    print(f"Warning: expected lightmap file not found: {out_file_path}")

                rel_path = Path("lightmaps") / scene_stem / f"{lm_id}_{out_file}"
                scene_lm["lightmaps"].append({
                    "id": lm_id,
                    "file": str(rel_path).replace('\\', '/'),
                    "format": fmt,
                    "resolution": resolution if isinstance(resolution, list) or isinstance(resolution, int) else resolution,
                    "usage": "Lightmap",
                    "instanceId": iid
                })

                scene_lm["lightmapBindings"][iid] = {
                    "lightmapId": lm_id,
                    "uvChannel": 1,
                    "uvScale": [1.0, 1.0],
                    "uvOffset": [0.0, 0.0]
                }

                # Copy produced file into assets tree under out_base/<lm_id>_<filename>
                target_dir = out_base
                target_dir.mkdir(parents=True, exist_ok=True)
                try:
                    tgt = target_dir / f"{lm_id}_{out_file}"
                    if not args.dry_run:
                        if out_file_path.exists():
                            import shutil

                            shutil.copy2(out_file_path, tgt)
                        else:
                            print(f"Note: produced file does not exist (skipping copy): {out_file_path}")
                except Exception as e:
                    print(f"Warning: failed to copy produced lightmap: {e}")

                lm_counter += 1

    # Write scene_lightmaps.json manifest
    manifest_out = Path(args.manifest_out) if args.manifest_out else Path("assets/scenes") / f"{scene_stem}_lightmaps.json"
    manifest_out.parent.mkdir(parents=True, exist_ok=True)
    manifest_out.write_text(json.dumps(scene_lm, indent=2))
    print(f"Wrote scene lightmap manifest: {manifest_out}")

    print("Done")


if __name__ == "__main__":
    main()

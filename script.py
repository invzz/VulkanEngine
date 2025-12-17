import os
import json
import re

# ================= CONFIG =================

OBJ_PATH = "./DM-Rankin/rankin.obj"
BASE_ASSET_DIR = "."
TEXTURE_INDEX_FILE = "texture_index.json"

SCALE_FACTOR = 0.01
APPLY_SCALE_IF_NO_MTLLIB = True

# =========================================


def normalize(name: str) -> str:
    return name.split(".", 1)[0].lower()


# ------------------------------------------------
# TEXTURE INDEX
# ------------------------------------------------

def load_or_build_texture_index():
    if os.path.exists(TEXTURE_INDEX_FILE):
        with open(TEXTURE_INDEX_FILE, "r", encoding="utf-8") as f:
            return json.load(f)

    index = {}
    for root, _, files in os.walk(BASE_ASSET_DIR):
        for f in files:
            if f.lower().endswith((".png", ".tga", ".jpg")):
                key = os.path.splitext(f)[0].lower()
                rel = os.path.relpath(
                    os.path.join(root, f),
                    os.path.dirname(OBJ_PATH)
                ).replace("\\", "/")
                index[key] = rel

    with open(TEXTURE_INDEX_FILE, "w", encoding="utf-8") as f:
        json.dump(index, f, indent=2)

    return index


texture_index = load_or_build_texture_index()

# ------------------------------------------------
# SHADER PARSING
# ------------------------------------------------

def parse_mat_file(path):
    data = {}
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if "=" in line:
                k, v = line.strip().split("=", 1)
                data[k.strip()] = v.strip()
    return data


def parse_props_file(path):
    data = {}
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if "=" in line and "Texture'" in line:
                k, v = line.split("=", 1)
                tex = re.search(r"Texture'([^']+)'", v)
                if tex:
                    data[k.strip()] = tex.group(1).split(".")[-1]
    return data


def resolve_shader(material_base):
    for root, _, files in os.walk(BASE_ASSET_DIR):
        for f in files:
            if f.lower() == f"{material_base}.props.txt":
                return parse_props_file(os.path.join(root, f))
            if f.lower() == f"{material_base}.mat":
                return parse_mat_file(os.path.join(root, f))
    return {}


# ------------------------------------------------
# OBJ PARSE
# ------------------------------------------------

materials = set()
has_mtllib = False

with open(OBJ_PATH, "r", encoding="utf-8") as f:
    for line in f:
        if line.startswith("mtllib"):
            has_mtllib = True
        elif line.startswith("usemtl"):
            materials.add(line.split(None, 1)[1].strip())

# ------------------------------------------------
# WRITE MTL
# ------------------------------------------------

mtl_path = os.path.splitext(OBJ_PATH)[0] + ".mtl"
missing_log = os.path.splitext(OBJ_PATH)[0] + "_missing_materials.txt"
missing = []

with open(mtl_path, "w", encoding="utf-8") as mtl:
    for mat in sorted(materials):
        base = normalize(mat)
        shader = resolve_shader(base)

        diffuse = shader.get("Diffuse") or shader.get("DiffuseTexture")
        opacity = shader.get("Opacity")

        if diffuse:
            diffuse = diffuse.lower()

        if opacity:
            opacity = opacity.lower()

        mtl.write(f"newmtl {mat}\n")
        mtl.write("Ka 1 1 1\n")
        mtl.write("Kd 1 1 1\n")
        mtl.write("Ks 0 0 0\n")
        mtl.write("illum 2\n")

        if diffuse and diffuse in texture_index:
            mtl.write(f"map_Kd {texture_index[diffuse]}\n")
        elif base in texture_index:
            mtl.write(f"map_Kd {texture_index[base]}\n")
        else:
            missing.append(mat)

        if opacity and opacity in texture_index:
            mtl.write(f"map_d {texture_index[opacity]}\n")

        mtl.write("\n")

# ------------------------------------------------
# SCALE + MTLLIB
# ------------------------------------------------

if not has_mtllib:
    with open(OBJ_PATH, "r", encoding="utf-8") as f:
        lines = f.readlines()

    out = [f"mtllib {os.path.basename(mtl_path)}\n"]

    for l in lines:
        if l.startswith("v "):
            p = l.split()
            v = [float(x) * SCALE_FACTOR for x in p[1:4]]
            out.append(f"v {v[0]} {v[1]} {v[2]}\n")
        else:
            out.append(l)

    with open(OBJ_PATH, "w", encoding="utf-8") as f:
        f.writelines(out)

# ------------------------------------------------
# LOG MISSING
# ------------------------------------------------

if missing:
    with open(missing_log, "w", encoding="utf-8") as f:
        for m in sorted(missing):
            f.write(m + "\n")

print("✅ Material resolution complete")

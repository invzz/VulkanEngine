import os
import re
import json
import argparse
import sys

def load_config():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "mtl_creator_config.json")

    if os.path.exists(config_path):
        with open(config_path, "r", encoding="utf-8") as f:
            config = json.load(f)
    else:
        print(f"Warning: Config file not found at {config_path}. Using defaults.")
        config = {"textures": {"base_path": script_dir, "cache_file": "texture_cache.json"}}
    
    # Ensure base_path is absolute
    texture_config = config.get("textures", {})
    base_path = texture_config.get("base_path", script_dir)
    if not os.path.isabs(base_path):
        texture_config["base_path"] = os.path.abspath(os.path.join(script_dir, base_path))
    
    return config

def get_cache_path(config):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    texture_config = config.get("textures", {})
    cache_filename = texture_config.get("cache_file", "texture_cache.json")
    
    if os.path.isabs(cache_filename):
        return cache_filename
    else:
        return os.path.join(script_dir, cache_filename)

def build_texture_cache(config):
    texture_config = config.get("textures", {})
    texture_base_path = texture_config.get("base_path")
    cache_path = get_cache_path(config)
    
    print(f"Building texture cache from: {texture_base_path}")
    texture_map = {}
    count = 0
    for root, dirs, files in os.walk(texture_base_path):
        for file in files:
            if file.lower().endswith(('.png', '.tga', '.jpg', '.jpeg', '.bmp', '.dds')):
                name = os.path.splitext(file)[0].lower()
                # Store absolute path
                texture_map[name] = os.path.join(root, file).replace("\\", "/")
                count += 1
    
    with open(cache_path, "w", encoding="utf-8") as f:
        json.dump(texture_map, f, indent=2, sort_keys=True)
    
    print(f"Cache built with {count} textures. Saved to {cache_path}")
    return texture_map

def load_texture_cache(config):
    cache_path = get_cache_path(config)
    if os.path.exists(cache_path):
        print(f"Loading texture cache from {cache_path}...")
        with open(cache_path, "r", encoding="utf-8") as f:
            return json.load(f)
    else:
        print("Cache not found. Building new cache...")
        return build_texture_cache(config)

def process_obj(obj_path, config):
    if not os.path.exists(obj_path):
        print(f"Error: File not found: {obj_path}")
        return

    print(f"Processing: {obj_path}")
    
    # Load cache
    texture_map = load_texture_cache(config)
    
    # Config for scaling
    target_unit = "meters"  # meters | centimeters | ue4 | uu
    scale_map = {"meters": 0.01905, "centimeters": 1.905, "ue4": 1.905, "uu": 1.0}
    scale_factor = scale_map[target_unit]

    # Output paths
    export_mtl_path = os.path.splitext(obj_path)[0] + ".mtl"
    log_missing_path = os.path.splitext(obj_path)[0] + "_missing_textures.txt"

    # === Step 1: Parse OBJ for materials and vertices ===
    materials_in_obj = set()
    vertices = []

    with open(obj_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        for line in lines:
            if line.startswith("usemtl "):
                mat_name = line.strip().split(" ", 1)[1]
                simple_name = re.split(r"[.\s]", mat_name)[0].lower()
                materials_in_obj.add((mat_name, simple_name))
            elif line.startswith("v "):
                vertices.append(line.strip())

    # Detect mtllib and scaling
    has_mtllib = any(l.startswith("mtllib ") for l in lines)
    already_scaled = any("UT2_SCALED_TO_" in l for l in lines)

    # === Step 2: Generate MTL file ===
    missing_textures = []

    with open(export_mtl_path, 'w', encoding='utf-8') as mtl_file:
        for full_name, simple_name in materials_in_obj:
            mtl_file.write(f"newmtl {full_name}\n")
            mtl_file.write("Ka 1.000 1.000 1.000\n")
            mtl_file.write("Kd 1.000 1.000 1.000\n")
            mtl_file.write("Ks 0.000 0.000 0.000\n")
            mtl_file.write("d 1.0\n")
            mtl_file.write("illum 2\n")

            # Assign texture if found
            abs_tex_path = texture_map.get(simple_name)
            if abs_tex_path:
                # Convert to relative path from OBJ location
                try:
                    rel_path = os.path.relpath(abs_tex_path, os.path.dirname(obj_path)).replace("\\", "/")
                    mtl_file.write(f"map_Kd {rel_path}\n")
                except ValueError:
                    # Fallback if on different drives on Windows, etc.
                    mtl_file.write(f"map_Kd {abs_tex_path}\n")
            else:
                missing_textures.append(full_name)
            mtl_file.write("\n")

    # === Step 3: Update OBJ for mtllib ===
    if not has_mtllib:
        lines.insert(0, f"mtllib {os.path.basename(export_mtl_path)}\n")

    # === Step 4: Optional scaling ===
    if not has_mtllib and not already_scaled and scale_factor != 1.0:
        scaled_lines = []
        scale_marker = f"# UT2_SCALED_TO_{target_unit.upper()} = {scale_factor}"
        scaled_lines.append(scale_marker + "\n")
        for line in lines:
            if line.startswith("v "):
                p = line.split()
                v = [float(x) * scale_factor for x in p[1:4]]
                scaled_lines.append(f"v {v[0]} {v[1]} {v[2]}\n")
            else:
                scaled_lines.append(line)
        lines = scaled_lines

    with open(obj_path, 'w', encoding='utf-8') as f:
        f.writelines(lines)

    # === Step 5: Log missing textures ===
    if missing_textures:
        with open(log_missing_path, 'w', encoding='utf-8') as log_file:
            for mat in missing_textures:
                log_file.write(mat + "\n")
        print(f"Missing textures logged to: {log_missing_path}")
    else:
        print("All materials have textures assigned.")

    print(f"MTL file created: {export_mtl_path}")
    print("OBJ updated to reference MTL and scaled (if needed).")

def main():
    parser = argparse.ArgumentParser(description="Create MTL file for OBJ and optionally scale it.")
    parser.add_argument("obj_path", nargs="?", help="Path to the OBJ file")
    args = parser.parse_args()

    config = load_config()

    if args.obj_path:
        process_obj(args.obj_path, config)
    else:
        while True:
            print("\n=== MTL Creator Menu ===")
            print("1. Create/Rebuild Texture Cache")
            print("2. Create MTL for a file")
            print("3. Exit")
            choice = input("Select an option: ").strip()
            
            if choice == "1":
                build_texture_cache(config)
            elif choice == "2":
                path = input("Enter OBJ file path: ").strip()
                # Remove quotes if user dragged and dropped file
                path = path.strip('"').strip("'")
                if path:
                    process_obj(path, config)
            elif choice == "3":
                print("Exiting.")
                break
            else:
                print("Invalid option.")

if __name__ == "__main__":
    main()

import sys

OBJ_PATH = "DM-Rankin/rankin.obj"
RECOVER_SCALE = 100.0

out = []

with open(OBJ_PATH, "r", encoding="utf-8") as f:
    for line in f:
        if line.startswith("v "):
            p = line.split()
            v = [float(x) * RECOVER_SCALE for x in p[1:4]]
            out.append(f"v {v[0]} {v[1]} {v[2]}\n")
        else:
            out.append(line)

with open(OBJ_PATH, "w", encoding="utf-8") as f:
    f.writelines(out)

print("✔ OBJ scale restored")

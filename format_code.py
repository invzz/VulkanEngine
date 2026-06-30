#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess

excluded = {"build", ".xmake", "third_party"}
files = []

for root, dirs, filenames in os.walk("."):
    dirs[:] = [d for d in dirs if d not in excluded]

    for name in filenames:
        if name.endswith((".cpp", ".hpp")):
            files.append(os.path.join(root, name))

if files:
    subprocess.run(["clang-format", "-i", *files], check=True)
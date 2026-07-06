#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# ANSI colors
RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
VIOLET = "\033[0;35m"
NC = "\033[0m"


def run_glslc(cmd):
    result = subprocess.run(cmd)
    return result.returncode == 0


def compile_shader(shader, output_file, include_args, target_arg):
    cmd = [
        "glslc",
        target_arg,
        *include_args,
        str(shader),
        "-o",
        str(output_file),
    ]

    if run_glslc(cmd):
        print(
            f"[ {GREEN}OK{NC} ] "
            f"{shader} -> {VIOLET}{output_file}{NC}"
        )
        return True

    print(f"[ {RED}Failed to compile{NC} ] {shader}")
    return False


def compile_standard_variant(shader, output_file, include_args):
    cmd = [
        "glslc",
        "--target-spv=spv1.5",
        "-DPBR_ENABLE_DEBUG=0",
        "-DPBR_ENABLE_SPEC_GLOSS=0",
        "-DPBR_ENABLE_IRIDESCENCE=0",
        "-DPBR_ENABLE_TRANSMISSION=0",
        "-DPBR_ENABLE_CLEARCOAT=0",
        "-DPBR_ENABLE_ANISOTROPY=0",
        *include_args,
        str(shader),
        "-o",
        str(output_file),
    ]

    if run_glslc(cmd):
        print(
            f"[ {GREEN}Variant{NC} ] "
            f"{shader} -> {VIOLET}{output_file}{NC}"
        )
    else:
        print(f"[ {RED}Failed variant{NC} ] {shader}")


def main():
    parser = argparse.ArgumentParser(
        description="Compile GLSL shaders to SPIR-V."
    )

    parser.add_argument(
        "-d",
        "--shader-dir",
        default="assets/shaders",
        help="Input shader directory",
    )

    parser.add_argument(
        "-o",
        "--output-dir",
        default="assets/shaders/compiled",
        help="Output directory",
    )

    args = parser.parse_args()

    if shutil.which("glslc") is None:
        print(f"{RED}Error: glslc not found in PATH.{NC}")
        return 1

    shader_dir = Path(args.shader_dir)
    output_dir = Path(args.output_dir)

    output_dir.mkdir(parents=True, exist_ok=True)

    include_args = [
        "-I",
        str(shader_dir),
        "-I",
        str(shader_dir / "includes"),
    ]

    build_standard_variant = os.environ.get(
        "ENGINE_BUILD_STANDARD_VARIANT",
        "1",
    )

    # Vertex shaders
    for shader in sorted(shader_dir.glob("*.vert")):
        output_file = output_dir / f"{shader.stem}.vert.spv"
        compile_shader(
            shader,
            output_file,
            include_args,
            "--target-spv=spv1.5",
        )

    # Fragment shaders
    for shader in sorted(shader_dir.glob("*.frag")):
        output_file = output_dir / f"{shader.stem}.frag.spv"

        compile_shader(
            shader,
            output_file,
            include_args,
            "--target-spv=spv1.5",
        )

        # Raytracing variant for deferred_lighting.frag
        if shader.name == "deferred_lighting.frag":
            rt_output = output_dir / "deferred_lighting_rt.frag.spv"
            cmd = [
                "glslc",
                "--target-spv=spv1.5",
                "-DRAY_TRACING_ENABLED=1",
                *include_args,
                str(shader),
                "-o",
                str(rt_output),
            ]
            if run_glslc(cmd):
                print(
                    f"[ {GREEN}RT{NC} ] "
                    f"{shader} -> {VIOLET}{rt_output}{NC}"
                )
            else:
                print(f"[ {RED}Failed RT{NC} ] {shader}")

        if shader.name == "pbr_shader.frag":
            if build_standard_variant != "0":
                standard_out = (
                    output_dir / "pbr_shader_standard.frag.spv"
                )

                compile_standard_variant(
                    shader,
                    standard_out,
                    include_args,
                )
            else:
                print(
                    f"[ {YELLOW}Skip variant{NC} ] "
                    f"{shader} "
                    "(ENGINE_BUILD_STANDARD_VARIANT=0)"
                )

    # Compute shaders
    for shader in sorted(shader_dir.glob("*.comp")):
        output_file = output_dir / f"{shader.stem}.comp.spv"
        compile_shader(
            shader,
            output_file,
            include_args,
            "--target-spv=spv1.5",
        )

    # Task shaders
    for shader in sorted(shader_dir.glob("*.task")):
        output_file = output_dir / f"{shader.stem}.task.spv"
        compile_shader(
            shader,
            output_file,
            include_args,
            "--target-env=vulkan1.3",
        )

    # Mesh shaders
    for shader in sorted(shader_dir.glob("*.mesh")):
        output_file = output_dir / f"{shader.stem}.mesh.spv"
        compile_shader(
            shader,
            output_file,
            include_args,
            "--target-env=vulkan1.3",
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())

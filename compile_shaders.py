#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor
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


def run_task(task):
    cmd, ok_msg, fail_msg = task
    return (True, ok_msg) if run_glslc(cmd) else (False, fail_msg)


def collect_tasks(shader_dir, output_dir, include_args,
                  build_standard_variant):
    tasks = []

    def add(cmd, ok_msg, fail_msg):
        tasks.append((cmd, ok_msg, fail_msg))

    def base_cmd(target_arg, shader, output_file, extra_defines=()):
        return [
            "glslc",
            target_arg,
            *extra_defines,
            *include_args,
            str(shader),
            "-o",
            str(output_file),
        ]

    # Vertex shaders
    for shader in sorted(shader_dir.glob("*.vert")):
        output_file = output_dir / f"{shader.stem}.vert.spv"
        cmd = base_cmd("--target-spv=spv1.5", shader, output_file)
        add(
            cmd,
            f"[ {GREEN}OK{NC} ] {shader} -> {VIOLET}{output_file}{NC}",
            f"[ {RED}Failed to compile{NC} ] {shader}",
        )

    # Fragment shaders
    for shader in sorted(shader_dir.glob("*.frag")):
        output_file = output_dir / f"{shader.stem}.frag.spv"
        cmd = base_cmd("--target-spv=spv1.5", shader, output_file)
        add(
            cmd,
            f"[ {GREEN}OK{NC} ] {shader} -> {VIOLET}{output_file}{NC}",
            f"[ {RED}Failed to compile{NC} ] {shader}",
        )

        # Raytracing variant for deferred_lighting.frag
        if shader.name == "deferred_lighting.frag":
            rt_output = output_dir / "deferred_lighting_rt.frag.spv"
            cmd = base_cmd(
                "--target-spv=spv1.5",
                shader,
                rt_output,
                extra_defines=("-DRAY_TRACING_ENABLED=1",),
            )
            add(
                cmd,
                f"[ {GREEN}RT{NC} ] {shader} -> {VIOLET}{rt_output}{NC}",
                f"[ {RED}Failed RT{NC} ] {shader}",
            )

        if shader.name == "pbr_shader.frag":
            if build_standard_variant != "0":
                standard_out = output_dir / "pbr_shader_standard.frag.spv"
                cmd = base_cmd(
                    "--target-spv=spv1.5",
                    shader,
                    standard_out,
                    extra_defines=(
                        "-DPBR_ENABLE_DEBUG=0",
                        "-DPBR_ENABLE_SPEC_GLOSS=0",
                        "-DPBR_ENABLE_IRIDESCENCE=0",
                        "-DPBR_ENABLE_TRANSMISSION=0",
                        "-DPBR_ENABLE_CLEARCOAT=0",
                        "-DPBR_ENABLE_ANISOTROPY=0",
                    ),
                )
                add(
                    cmd,
                    f"[ {GREEN}Variant{NC} ] {shader} -> {VIOLET}{standard_out}{NC}",
                    f"[ {RED}Failed variant{NC} ] {shader}",
                )
            else:
                print(
                    f"[ {YELLOW}Skip variant{NC} ] "
                    f"{shader} (ENGINE_BUILD_STANDARD_VARIANT=0)"
                )

    # Compute shaders
    for shader in sorted(shader_dir.glob("*.comp")):
        output_file = output_dir / f"{shader.stem}.comp.spv"
        cmd = base_cmd("--target-spv=spv1.5", shader, output_file)
        add(
            cmd,
            f"[ {GREEN}OK{NC} ] {shader} -> {VIOLET}{output_file}{NC}",
            f"[ {RED}Failed to compile{NC} ] {shader}",
        )

    # Task shaders
    for shader in sorted(shader_dir.glob("*.task")):
        output_file = output_dir / f"{shader.stem}.task.spv"
        cmd = base_cmd("--target-env=vulkan1.3", shader, output_file)
        add(
            cmd,
            f"[ {GREEN}OK{NC} ] {shader} -> {VIOLET}{output_file}{NC}",
            f"[ {RED}Failed to compile{NC} ] {shader}",
        )

    # Mesh shaders
    for shader in sorted(shader_dir.glob("*.mesh")):
        output_file = output_dir / f"{shader.stem}.mesh.spv"
        cmd = base_cmd("--target-env=vulkan1.3", shader, output_file)
        add(
            cmd,
            f"[ {GREEN}OK{NC} ] {shader} -> {VIOLET}{output_file}{NC}",
            f"[ {RED}Failed to compile{NC} ] {shader}",
        )

    return tasks


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

    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=os.cpu_count() or 1,
        help="Number of parallel glslc invocations",
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

    tasks = collect_tasks(
        shader_dir, output_dir, include_args, build_standard_variant
    )

    if not tasks:
        return 0

    failed = False
    with ProcessPoolExecutor(max_workers=args.jobs) as executor:
        for ok, message in executor.map(run_task, tasks):
            print(message)
            if not ok:
                failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

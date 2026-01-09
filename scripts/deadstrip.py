#!/usr/bin/env python3
"""Capture and report link-time dead-stripping.

Primary target: MSVC (Windows) using link.exe /VERBOSE:REF.

What it does (Windows/MSVC):
  1) Runs xmake configure/build with --deadcode=y so the link step includes /VERBOSE:REF.
  2) Extracts the final link.exe command line for <target>.exe from the xmake -vD log.
  3) Re-runs link.exe under a VS Developer environment and captures full /VERBOSE:REF output.
  4) Parses the verbose log and writes a markdown report.

Usage:
  python scripts/deadstrip.py --project-root . --target Cube --mode release --rebuild

Outputs (defaults):
  - xmake log:   <root>/xmake_link_diag_<target>_<mode>.log
  - link rsp:    <root>/link_<target>_verbose.rsp
  - link log:    <root>/link_<target>_verbose.log
  - md report:   <root>/deadstrip_report.md

Notes:
  - This script is designed to be standard-library only.
  - Non-Windows platforms: this script can still generate the markdown report if you
    already have a log that contains "Discarded ... from ..." lines (pass --log-only).
"""

from __future__ import annotations

import argparse
import os
import platform
import re
import shlex
import subprocess
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
LINK_LINE_RE = re.compile(
    r'^"(?P<linkexe>[^"]+\\link\.exe)"\s+(?P<args>.+)$', re.IGNORECASE
)
DISCARDED_RE = re.compile(r"^\s*Discarded\s+.+\s+from\s+(.+)$")

NOISE_PATTERNS = [
    re.compile(r"^\s*Discarded\s+`string`\x27"),
    re.compile(r"__xmm@"),
    re.compile(r"__real@"),
    re.compile(r"in_place_type"),
    re.compile(r"\x60vftable\x60"),
    re.compile(r"\?\?__C@_"),
]


def strip_ansi(s: str) -> str:
    return ANSI_RE.sub("", s)


def iter_lines(path: Path) -> Iterable[str]:
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            yield line.rstrip("\n")


def is_noise_line(line: str) -> bool:
    return any(p.search(line) for p in NOISE_PATTERNS)


def normalize_from_token(from_token: str) -> str:
    return from_token.replace("\\", "/").strip()


def is_project_local(from_token: str) -> bool:
    # Treat Engine.lib + any split Engine*.lib modules as project-local.
    # e.g. Engine.lib(Foo.obj), EngineImporters.lib(Bar.obj), EngineSceneIO.lib(Baz.obj)
    if from_token.startswith("Engine") and ".lib(" in from_token:
        return True
    # Demo objs are also project-local, e.g. app.cpp.obj
    if from_token.endswith(".cpp.obj") and ".lib(" not in from_token:
        return True
    return False


def get_from_token(line: str) -> Optional[str]:
    m = DISCARDED_RE.match(line)
    if not m:
        return None
    return normalize_from_token(m.group(1))


def write_markdown(
    out_path: Path,
    source_log_name: str,
    groups_filtered: list[tuple[str, int]],
    groups_project_filtered: list[tuple[str, int]],
    groups_external_filtered: list[tuple[str, int]],
    sample_lines_by_from: dict[str, list[str]],
) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)

    md: list[str] = []
    md.append("# Dead-strip report (MSVC link /VERBOSE:REF)")
    md.append("")
    md.append(f"Source log: {source_log_name}")
    md.append("")

    def add_table(title: str, rows: list[tuple[str, int]], limit: int = 25) -> None:
        md.append(f"## {title}")
        md.append("")
        md.append("| From | Discarded lines |")
        md.append("| --- | ---: |")
        for from_token, count in rows[:limit]:
            md.append(f"| {from_token} | {count} |")
        md.append("")

    add_table("Top contributors (filtered)", groups_filtered)
    add_table("Project-only (filtered)", groups_project_filtered)
    add_table("External (filtered)", groups_external_filtered)

    md.append("## Sample discarded symbols (filtered, project-only)")
    md.append("")
    for from_token, _count in groups_project_filtered[:10]:
        md.append(f"### {from_token}")
        for line in sample_lines_by_from.get(from_token, [])[:15]:
            md.append(f"- {line.strip()}")
        md.append("")

    out_path.write_text("\n".join(md) + "\n", encoding="utf-8")


def generate_report_from_log(log_path: Path, out_md: Path) -> None:
    if not log_path.exists():
        raise FileNotFoundError(f"Missing log file: {log_path}")

    counts_filtered: dict[str, int] = defaultdict(int)
    sample_lines_by_from: dict[str, list[str]] = defaultdict(list)

    for line in iter_lines(log_path):
        if not DISCARDED_RE.match(line):
            continue
        if is_noise_line(line):
            continue
        from_token = get_from_token(line)
        if not from_token:
            continue
        counts_filtered[from_token] += 1
        if is_project_local(from_token):
            sample_lines_by_from[from_token].append(line)

    groups_filtered = sorted(
        counts_filtered.items(), key=lambda kv: kv[1], reverse=True
    )
    groups_project_filtered = [
        (k, v) for (k, v) in groups_filtered if is_project_local(k)
    ]
    groups_external_filtered = [
        (k, v) for (k, v) in groups_filtered if not is_project_local(k)
    ]

    write_markdown(
        out_path=out_md,
        source_log_name=os.path.basename(str(log_path)),
        groups_filtered=groups_filtered,
        groups_project_filtered=groups_project_filtered,
        groups_external_filtered=groups_external_filtered,
        sample_lines_by_from=sample_lines_by_from,
    )


@dataclass
class LinkCommand:
    link_exe: str
    args_str: str
    command_line: str


def find_link_command_in_xmake_log(
    xmake_log: Path, target: str, mode: str
) -> LinkCommand:
    # xmake logs include the full link.exe command line for the final binary.
    # Note: keep this as a normal string (not a raw string) so we can safely represent
    # backslashes without accidentally triggering Python's \xNN escape sequences.
    out_needle = f"-out:build\\windows\\x64\\{mode}\\{target}.exe"

    cmd_line: Optional[str] = None
    for raw in iter_lines(xmake_log):
        line = strip_ansi(raw)
        if "link.exe" not in line.lower():
            continue
        if out_needle not in line:
            continue
        cmd_line = line

    if not cmd_line:
        raise RuntimeError(
            f"Could not find link.exe invocation for {target}.exe in {xmake_log}"
        )

    m = LINK_LINE_RE.match(cmd_line)
    if not m:
        raise RuntimeError("Could not parse link.exe command line from xmake log")

    return LinkCommand(
        link_exe=m.group("linkexe"), args_str=m.group("args"), command_line=cmd_line
    )


def quote_if_needed(token: str) -> str:
    if any(c.isspace() for c in token):
        return '"' + token.replace('"', '\\"') + '"'
    return token


def write_rsp(args_tokens: list[str], rsp_path: Path) -> None:
    # One argument per line; quote tokens with spaces.
    lines = [quote_if_needed(t) for t in args_tokens if t]
    rsp_path.write_text("\n".join(lines) + "\n", encoding="ascii", errors="ignore")


def run_xmake(
    project_root: Path, target: str, mode: str, rebuild: bool, xmake_log: Path
) -> None:
    xmake_log.parent.mkdir(parents=True, exist_ok=True)

    cfg_cmd = ["xmake", "f", "-P", str(project_root), "-m", mode, "--deadcode=y"]
    build_cmd = ["xmake", "-P", str(project_root), "-vD"]
    if rebuild:
        build_cmd.append("-r")
    build_cmd.append(target)

    with xmake_log.open("w", encoding="utf-8", errors="replace") as f:
        subprocess.run(cfg_cmd, stdout=f, stderr=subprocess.STDOUT, check=False)
        subprocess.run(build_cmd, stdout=f, stderr=subprocess.STDOUT, check=True)


def kill_running_exe(target: str) -> None:
    # Avoid LNK1104 if the exe is running.
    # Ignore failure (taskkill returns non-zero if process not found).
    try:
        subprocess.run(
            ["taskkill", "/F", "/IM", f"{target}.exe"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except FileNotFoundError:
        pass


def run_link_verbose(
    project_root: Path,
    link_cmd: LinkCommand,
    rsp_path: Path,
    link_log: Path,
    vsdevcmd: Path,
    vs_arch_args: str,
) -> None:
    link_log.parent.mkdir(parents=True, exist_ok=True)
    if link_log.exists():
        link_log.unlink()

    if not vsdevcmd.exists():
        raise FileNotFoundError(f"VsDevCmd not found: {vsdevcmd}")

    # Use a temporary .cmd file to avoid fragile quoting/escaping issues.
    # xmake logs escape paths as C:\\Program Files\\..., normalize that back to C:\Program Files\...
    link_exe = link_cmd.link_exe.replace("\\\\", "\\")

    cmd_path = project_root / f".deadstrip_link_{rsp_path.stem}.cmd"
    cmd_contents = "\r\n".join(
        [
            "@echo off",
            f'call "{vsdevcmd}" {vs_arch_args} >nul',
            f'"{link_exe}" @"{rsp_path}" > "{link_log}" 2>&1',
            "exit /b %errorlevel%",
            "",
        ]
    )
    cmd_path.write_text(cmd_contents, encoding="utf-8")

    try:
        subprocess.run(["cmd", "/c", str(cmd_path)], cwd=str(project_root), check=True)
    finally:
        # Best-effort cleanup.
        try:
            cmd_path.unlink()
        except OSError:
            pass


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent

    parser = argparse.ArgumentParser(
        description="Capture and report link-time dead-stripping"
    )
    parser.add_argument(
        "--project-root",
        default=str(repo_root),
        help="Project root (default: repo root)",
    )
    parser.add_argument(
        "--target", default="Cube", help="xmake target/exe name (default: Cube)"
    )
    parser.add_argument(
        "--mode",
        choices=("release", "debug"),
        default="release",
        help="Build mode (default: release)",
    )
    parser.add_argument("--rebuild", action="store_true", help="Force rebuild/relink")

    parser.add_argument(
        "--vsdevcmd",
        default=r"C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\Tools\VsDevCmd.bat",
        help="Path to VsDevCmd.bat (Windows/MSVC only)",
    )
    parser.add_argument(
        "--vs-arch-args",
        default="-arch=amd64 -host_arch=amd64",
        help="Args passed to VsDevCmd.bat (default: %(default)s)",
    )

    parser.add_argument(
        "--log-only",
        default="",
        help="Skip capture; only generate markdown from an existing verbose log at this path",
    )
    parser.add_argument(
        "--out",
        default=str(repo_root / "deadstrip_report.md"),
        help="Markdown output path (default: %(default)s)",
    )

    args = parser.parse_args()

    project_root = Path(args.project_root).resolve()
    target = args.target
    mode = args.mode

    out_md = Path(args.out).resolve()

    # Log-only mode: generate report from an already-captured link verbose log.
    if args.log_only:
        log_path = Path(args.log_only).resolve()
        generate_report_from_log(log_path, out_md)
        print(f"Wrote {out_md}")
        return 0

    if platform.system().lower() != "windows":
        raise SystemExit(
            "Capture mode is currently supported only on Windows/MSVC. Use --log-only to generate a report from an existing log."
        )

    xmake_log = project_root / f"xmake_link_diag_{target}_{mode}.log"
    rsp_path = project_root / f"link_{target}_verbose.rsp"
    link_log = project_root / f"link_{target}_verbose.log"

    # 1) Build with xmake so we can scrape the final link.exe command line.
    print(f"[1/3] Building {target} ({mode}) with xmake -vD ...")
    run_xmake(
        project_root,
        target=target,
        mode=mode,
        rebuild=args.rebuild,
        xmake_log=xmake_log,
    )

    # 2) Extract the link.exe command line. If not found, retry with rebuild once.
    print("[2/3] Extracting link.exe command and running link.exe /VERBOSE:REF ...")
    try:
        link_cmd = find_link_command_in_xmake_log(xmake_log, target=target, mode=mode)
    except RuntimeError:
        if not args.rebuild:
            run_xmake(
                project_root,
                target=target,
                mode=mode,
                rebuild=True,
                xmake_log=xmake_log,
            )
            link_cmd = find_link_command_in_xmake_log(
                xmake_log, target=target, mode=mode
            )
        else:
            raise

    # Tokenize args and write response file.
    args_tokens = shlex.split(link_cmd.args_str, posix=False)
    write_rsp(args_tokens, rsp_path)

    kill_running_exe(target)

    # Re-run link.exe under VS dev env and capture full verbose output.
    run_link_verbose(
        project_root=project_root,
        link_cmd=link_cmd,
        rsp_path=rsp_path,
        link_log=link_log,
        vsdevcmd=Path(args.vsdevcmd),
        vs_arch_args=args.vs_arch_args,
    )

    # 3) Generate report.
    print("[3/3] Generating markdown report ...")
    generate_report_from_log(link_log, out_md)

    print("Done.")
    print(f"- xmake log: {xmake_log}")
    print(f"- link rsp:  {rsp_path}")
    print(f"- link log:  {link_log}")
    print(f"- report:    {out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

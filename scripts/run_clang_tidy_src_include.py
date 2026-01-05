#!/usr/bin/env python3
"""Run clang-tidy on this repo, limited to src/ and include/.

Equivalent to:
  Get-ChildItem -Path @(".\\src", ".\\include") -Recurse -File -Include *.cpp,*.hpp |
        ForEach-Object { clang-tidy $_.FullName -p .\\.vscode -fix --format-style=file }

Notes:
- Assumes compile_commands.json is located under ./.vscode
- Designed to work on Windows PowerShell / cmd as well as other OSes
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Iterable, Sequence


DEFAULT_EXTENSIONS = (".cpp",)


def safe_print(*args, **kwargs) -> bool:
    """print() that exits cleanly if stdout is closed (e.g. piped to `Select-Object -First`)."""
    try:
        print(*args, **kwargs)
        return True
    except BrokenPipeError:
        # If the consumer closes the pipe early, don't treat it as an error.
        try:
            sys.stdout.close()
        except Exception:
            pass
        return False


def _existing_dir(path: Path, label: str) -> Path:
    if not path.exists():
        raise SystemExit(f"error: {label} does not exist: {path}")
    if not path.is_dir():
        raise SystemExit(f"error: {label} is not a directory: {path}")
    return path


def collect_files(root: Path, extensions: Sequence[str]) -> list[Path]:
    roots = [
        _existing_dir(root / "src", "src directory"),
        _existing_dir(root / "include", "include directory"),
    ]

    exts = {e if e.startswith(".") else f".{e}" for e in extensions}

    results: list[Path] = []
    for base in roots:
        for p in base.rglob("*"):
            if p.is_file() and p.suffix.lower() in exts:
                results.append(p)

    # Stable order
    results.sort(key=lambda p: str(p).lower())
    return results


def build_clang_tidy_cmd(
    clang_tidy: str,
    file_path: Path,
    compile_commands_dir: Path,
    fix: bool,
    format_style: str | None,
    header_filter: str | None,
    system_headers: bool,
    extra_args: Sequence[str],
) -> list[str]:
    cmd = [clang_tidy, str(file_path), "-p", str(compile_commands_dir)]
    if fix:
        cmd.append("-fix")
    if format_style is not None:
        cmd.append(f"--format-style={format_style}")
    if header_filter is not None:
        cmd.append(f"-header-filter={header_filter}")
    if system_headers:
        cmd.append("-system-headers")
    cmd.extend(extra_args)
    return cmd


def _escape_for_clang_tidy_regex(path: str) -> str:
    # clang-tidy uses LLVM regex (ECMAScript-like). Escape regex metacharacters.
    # We also want to match both '\\' and '/' path separators on Windows.
    escaped = ""
    for ch in path:
        if ch in r"\\.^$|?*+()[]{}":
            escaped += "\\" + ch
        else:
            escaped += ch
    # Replace literal backslashes with a class that matches either slash style.
    escaped = escaped.replace(r"\\\\", r"[\\\\/]")
    return escaped


def run_one(cmd: Sequence[str]) -> tuple[int, str]:
    # Capture output so parallel runs don't interleave too badly; print on failure.
    proc = subprocess.run(cmd, text=True, capture_output=True)
    combined = ""
    if proc.stdout:
        combined += proc.stdout
    if proc.stderr:
        combined += proc.stderr
    return proc.returncode, combined


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description="Run clang-tidy on src/ and include/ only")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path.cwd(),
        help="Repository root (defaults to current working directory)",
    )
    parser.add_argument(
        "--compile-commands",
        type=Path,
        default=Path(".vscode"),
        help="Directory that contains compile_commands.json (default: .vscode)",
    )
    parser.add_argument(
        "--clang-tidy",
        default="clang-tidy",
        help="clang-tidy executable name/path (default: clang-tidy)",
    )
    parser.add_argument(
        "--extensions",
        nargs="*",
        default=list(DEFAULT_EXTENSIONS),
        help="File extensions to run clang-tidy on (default: .cpp)",
    )
    parser.add_argument(
        "--include-headers",
        action="store_true",
        help="Also run clang-tidy directly on headers under include/ (not recommended)",
    )
    parser.add_argument("--no-fix", action="store_true", help="Do not pass -fix")
    parser.add_argument(
        "--format-style",
        default="file",
        help=(
            "Formatting style for code around applied fixes (default: file). "
            "Use 'none' to disable formatting."
        ),
    )
    parser.add_argument(
        "--no-format",
        action="store_true",
        help="Disable formatting around applied fixes (equivalent to --format-style=none)",
    )
    parser.add_argument(
        "--header-filter",
        default=None,
        help=(
            "Override clang-tidy -header-filter regex. "
            "Default restricts diagnostics/fixes to this repo's src/ and include/."
        ),
    )
    parser.add_argument(
        "--system-headers",
        action="store_true",
        help="Also report diagnostics in system headers (off by default)",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=1,
        help="Number of parallel clang-tidy processes (default: 1)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands but do not execute clang-tidy",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Limit to the first N files (0 = no limit)",
    )
    parser.add_argument(
        "extra",
        nargs=argparse.REMAINDER,
        help="Extra args passed to clang-tidy after '--' (example: -- -checks=...)",
    )

    args = parser.parse_args(list(argv))

    root = args.root.resolve()
    compile_commands_dir = (root / args.compile_commands).resolve()

    if not (compile_commands_dir / "compile_commands.json").exists():
        raise SystemExit(
            "error: compile_commands.json not found at "
            f"{compile_commands_dir / 'compile_commands.json'}\n"
            "hint: pass --compile-commands to point at the directory containing compile_commands.json"
        )

    files = collect_files(root, args.extensions)
    if not files:
        print("No matching files found under src/ or include/.")
        return 0

    # By default we only run on translation units (e.g. .cpp). Running clang-tidy
    # directly on headers tends to create noise and can lead to fix-its in external
    # dependency headers depending on include paths.
    if not args.include_headers:
        files = [p for p in files if p.suffix.lower() != ".hpp"]

    fix = not args.no_fix
    # clang-tidy has --format-style (default is 'none'). We default to 'file' for convenience.
    format_style: str | None = None
    if not args.no_format:
        # Allow explicit 'none' via --format-style none
        if str(args.format_style).lower() != "none":
            format_style = str(args.format_style)

    # Support passing extra args like: -- -checks=... -warnings-as-errors=...
    extra_args = list(args.extra)
    if extra_args and extra_args[0] == "--":
        extra_args = extra_args[1:]

    jobs = max(1, int(args.jobs))

    limit = max(0, int(args.limit))
    if limit:
        files = files[:limit]

    if not safe_print(f"Root: {root}"):
        return 0
    if not safe_print(f"Compile commands: {compile_commands_dir}"):
        return 0
    if not safe_print(f"Files: {len(files)}"):
        return 0
    if not safe_print(f"Parallel jobs: {jobs}"):
        return 0

    # Restrict diagnostics and fixes in headers to project-owned paths only.
    # This prevents clang-tidy from touching cached/third-party headers (e.g. xmake packages).
    if args.header_filter is None:
        root_pattern = _escape_for_clang_tidy_regex(str(root))
        header_filter = rf"^{root_pattern}[\\\\/](src|include)[\\\\/].*"
    else:
        header_filter = str(args.header_filter)

    def mkcmd(p: Path) -> list[str]:
        return build_clang_tidy_cmd(
            clang_tidy=args.clang_tidy,
            file_path=p,
            compile_commands_dir=compile_commands_dir,
            fix=fix,
            format_style=format_style,
            header_filter=header_filter,
            system_headers=bool(args.system_headers),
            extra_args=extra_args,
        )

    if args.dry_run:
        for p in files:
            if not safe_print(" ".join(mkcmd(p))):
                return 0
        return 0

    failures: list[tuple[Path, int, str]] = []

    if jobs == 1:
        for i, p in enumerate(files, start=1):
            cmd = mkcmd(p)
            if not safe_print(f"[{i}/{len(files)}] {p}"):
                return 0
            code, out = run_one(cmd)
            if code != 0:
                failures.append((p, code, out))
                # Print output immediately for easier diagnosis.
                if out.strip():
                    sys.stderr.write(out)
        
    else:
        # Run a few at a time to keep the progress readable.
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            future_to_path = {ex.submit(run_one, mkcmd(p)): p for p in files}
            done_count = 0
            for fut in as_completed(future_to_path):
                p = future_to_path[fut]
                done_count += 1
                try:
                    code, out = fut.result()
                except Exception as e:  # pragma: no cover
                    failures.append((p, 1, str(e)))
                    continue

                if not safe_print(f"[{done_count}/{len(files)}] {p}"):
                    return 0
                if code != 0:
                    failures.append((p, code, out))
                    if out.strip():
                        sys.stderr.write(out)

    if failures:
        print(f"\nclang-tidy finished with {len(failures)} failures.", file=sys.stderr)
        # Return a non-zero code suitable for CI/scripts.
        return 1

    safe_print("\nclang-tidy finished successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

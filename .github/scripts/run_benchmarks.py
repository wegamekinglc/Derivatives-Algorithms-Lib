#!/usr/bin/env python3
"""Smoke-run CTest benchmark commands and retain per-test output and status."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

from write_benchmark_summary import build_report


def valid_test_name(name: str) -> bool:
    return bool(name) and name not in (".", "..") and "/" not in name and "\\" not in name


def discover_benchmarks(build_root: Path, results_dir: Path) -> list[dict]:
    completed = subprocess.run(
        ["ctest", "--test-dir", str(build_root), "--show-only=json-v1", "-L", "^benchmark$"],
        check=True, capture_output=True, text=True,
    )
    (results_dir / "ctest-tests.json").write_text(completed.stdout, encoding="utf-8")
    tests = json.loads(completed.stdout)["tests"]
    if not tests:
        raise ValueError("CTest discovered no benchmark tests")
    names = set()
    for test in tests:
        name = test["name"]
        if not valid_test_name(name) or name in names:
            raise ValueError(f"invalid or duplicate benchmark test name: {name!r}")
        if not test.get("command"):
            raise ValueError(f"CTest could not resolve the command for {name}; build the benchmark first")
        names.add(name)
    return tests


def run_benchmark(test: dict, build_root: Path, results_dir: Path, resource_usage: bool) -> int:
    name = test["name"]
    properties = {item["name"]: item["value"] for item in test.get("properties", [])}
    command = test["command"]
    if resource_usage:
        command = ["/usr/bin/time", "--verbose", f"--output={results_dir / (name + '.resources.txt')}", *command]
    output_file = results_dir / f"{name}.txt"
    print(f"========== {name} ==========", flush=True)
    with output_file.open("w", encoding="utf-8") as output:
        try:
            # CTest supplies the locally generated argument vector; no shell parses it.
            completed = subprocess.run(  # nosemgrep
                command, cwd=properties.get("WORKING_DIRECTORY", str(build_root)),
                stdout=output, stderr=subprocess.STDOUT, check=False, shell=False,
            )
            status = completed.returncode
        except OSError as error:
            output.write(f"Could not run {name}: {error}\n")
            status = 127
    (results_dir / f"{name}.status").write_text(f"{status}\n", encoding="utf-8")
    print(output_file.read_text(encoding="utf-8", errors="replace"), flush=True)
    return status


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--results-dir", type=Path, required=True)
    parser.add_argument("--configuration", required=True)
    parser.add_argument("--summary-file", type=Path)
    parser.add_argument("--resource-usage", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    build_root = args.build_root.resolve()
    results_dir = args.results_dir.resolve()
    results_dir.mkdir(parents=True, exist_ok=True)
    try:
        tests = discover_benchmarks(build_root, results_dir)
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"Benchmark discovery failed: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr, file=sys.stderr)
        return 1
    statuses = [run_benchmark(test, build_root, results_dir, args.resource_usage) for test in tests]
    report = build_report([test["name"] for test in tests], results_dir, args.configuration)
    (results_dir / "summary.md").write_text(report, encoding="utf-8")
    if args.summary_file:
        with args.summary_file.open("a", encoding="utf-8") as summary:
            summary.write(report)
    print(report, end="")
    return int(any(statuses))


if __name__ == "__main__":
    raise SystemExit(main())

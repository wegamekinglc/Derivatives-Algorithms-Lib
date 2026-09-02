#!/usr/bin/env python3
"""Write reproducible, non-secret benchmark environment metadata."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import subprocess


def command_version(command: str) -> str:
    try:
        if command == "gcc-14":
            completed = subprocess.run(
                ["gcc-14", "--version"], check=False, capture_output=True, text=True, timeout=30
            )
        elif command == "cl":
            completed = subprocess.run(
                ["cl", "--version"], check=False, capture_output=True, text=True, timeout=30
            )
        elif command == "python3":
            completed = subprocess.run(
                ["python3", "--version"], check=False, capture_output=True, text=True, timeout=30
            )
        elif command == "cmake":
            completed = subprocess.run(
                ["cmake", "--version"], check=False, capture_output=True, text=True, timeout=30
            )
        else:
            return f"unsupported command: {command}"
    except (OSError, subprocess.TimeoutExpired) as error:
        return f"unavailable: {error}"
    output = f"{completed.stdout}\n{completed.stderr}"
    return next((line.strip() for line in output.splitlines() if line.strip()), "unavailable")


def cpu_model() -> str:
    identifier = os.environ.get("PROCESSOR_IDENTIFIER")
    if identifier:
        return identifier
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        for line in cpuinfo.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.lower().startswith("model name") and ":" in line:
                return line.split(":", maxsplit=1)[1].strip()
    return platform.processor() or platform.machine() or "unknown"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--head-sha", required=True)
    parser.add_argument("--base-sha", required=True)
    parser.add_argument("--compiler", required=True, choices=("gcc-14", "cl", "python3"))
    parser.add_argument("--aad-backend", required=True)
    parser.add_argument("--threads", required=True, type=int)
    parser.add_argument("--runner-image", required=True)
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--native-arch", choices=("on", "off"), default="on")
    parser.add_argument("--shared-libraries", choices=("on", "off"), default="off")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    metadata = {
        "schema": "dal.benchmark-environment/1",
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "head_sha": args.head_sha,
        "base_sha": args.base_sha,
        "build_type": args.build_type,
        "shared_libraries": args.shared_libraries,
        "native_arch": args.native_arch,
        "aad_backend": args.aad_backend,
        "dal_num_threads": args.threads,
        "cpu_model": cpu_model(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "runner_image": args.runner_image,
        "runner_os": os.environ.get("RUNNER_OS", platform.system()),
        "runner_arch": os.environ.get("RUNNER_ARCH", platform.machine()),
        "runner_name": os.environ.get("RUNNER_NAME", "local"),
        "github_event_name": os.environ.get("GITHUB_EVENT_NAME", "local"),
        "github_run_id": os.environ.get("GITHUB_RUN_ID", "local"),
        "github_run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", "local"),
        "compiler_command": args.compiler,
        "compiler_version": command_version(args.compiler),
        "cmake_command": "cmake",
        "cmake_version": command_version("cmake"),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

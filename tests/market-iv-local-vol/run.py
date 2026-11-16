"""Run the blocked S3 reference checkpoint; exit 2 means unqualified product."""

import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

import mpmath
import numpy
import scipy

from generate import request
from qualify import digest, qualify


def write_json(path, value):
    path.write_text(
        json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )


def command_output(command, root):
    return subprocess.run(
        command, cwd=root, check=True, capture_output=True, text=True
    ).stdout.strip()


def provenance(binary):
    source = Path(__file__).resolve().parent
    root = source.parent.parent
    return dict(
        head=command_output(["git", "rev-parse", "HEAD"], root),
        production_tree=command_output(["git", "rev-parse", "HEAD:dal-cpp/dal"], root),
        production_diff=command_output(
            ["git", "diff", "HEAD", "--", "dal-cpp/dal"], root
        ),
        submodules=command_output(["git", "submodule", "status"], root),
        platform=platform.platform(),
        python=sys.version,
        packages=dict(
            mpmath=mpmath.__version__, numpy=numpy.__version__, scipy=scipy.__version__
        ),
        binary_sha256=digest(binary),
        reference_sources={
            path.name: digest(path)
            for path in sorted(source.iterdir())
            if path.is_file()
        },
        library_sha256=digest(binary.parent / "workspace/dal-cpp/libdal_cpp.a"),
        compiler=command_output(["c++", "--version"], root),
        cmake_cache_sha256=digest(binary.parent / "CMakeCache.txt"),
    )


def run(binary, output, jobs):
    output.mkdir(parents=True, exist_ok=True)
    request_path, capture_path = output / "request.json", output / "capture.json"
    write_json(request_path, request())
    result = subprocess.run(
        [str(binary), str(request_path), str(capture_path)],
        check=False,
        capture_output=True,
        text=True,
    )
    (output / "capture.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode not in (0, 2):
        raise RuntimeError(f"Capture failed: exit {result.returncode}; see capture.log")
    report = qualify(request_path, capture_path, jobs)
    write_json(output / "qualification.json", report)
    metadata = provenance(binary)
    metadata.update(
        capture_exit=result.returncode,
        qualification_exit=2,
        request_sha256=digest(request_path),
        capture_sha256=digest(capture_path),
        qualification_sha256=digest(output / "qualification.json"),
    )
    write_json(output / "provenance.json", metadata)
    print(
        json.dumps(
            dict(
                status=report["status"],
                pde_studies=len(report["pde_studies"]),
                fixture_ids=[row["id"] for row in report["fixtures"]],
            )
        )
    )
    return 2


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    args = parser.parse_args()
    raise SystemExit(run(args.capture.resolve(), args.output.resolve(), args.jobs))

"""Run the independent reference phase; product qualification is separate."""

import argparse
import json
import os
from pathlib import Path
import platform
import sys

import mpmath
import numpy
import scipy

from qualify import digest, qualify


def write_json(path, value):
    path.write_text(
        json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )


def provenance(binary, evidence):
    source = Path(__file__).resolve().parent
    return dict(
        head=(evidence / "source-head.txt").read_text(encoding="utf-8").strip(),
        production_tree=(evidence / "production-tree.txt")
        .read_text(encoding="utf-8")
        .strip(),
        production_diff=(evidence / "production-diff.txt").read_text(encoding="utf-8"),
        submodules=(evidence / "submodules.txt").read_text(encoding="utf-8"),
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
        compiler=(evidence / "compiler.txt").read_text(encoding="utf-8"),
        cmake_cache_sha256=digest(binary.parent / "CMakeCache.txt"),
    )


def run(binary, output, jobs):
    output.mkdir(parents=True, exist_ok=True)
    request_path, capture_path = output / "request.json", output / "capture.json"
    report = qualify(request_path, capture_path, jobs)
    write_json(output / "qualification.json", report)
    metadata = provenance(binary, output)
    metadata.update(
        capture_exit=int((output / "capture-exit.txt").read_text(encoding="utf-8")),
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

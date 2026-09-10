#!/usr/bin/env python3
"""Load one explicitly selected DAL build in a fresh benchmark process."""

import argparse
from importlib.machinery import EXTENSION_SUFFIXES
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--inventory", action="store_true")
    args = parser.parse_args()
    package = args.package.resolve()
    if not (package / "dal/__init__.py").is_file():
        raise ValueError(f"build-tree DAL package missing: {package}")
    sys.path[:0] = [str(package), str(args.suite.resolve())]
    import dal

    if not any(
        str(dal._dal.__file__).endswith(suffix) for suffix in EXTENSION_SUFFIXES
    ):
        raise ValueError("DAL benchmark requires the compiled native extension")
    for path in (dal.__file__, dal._dal.__file__):
        if not Path(path).resolve().is_relative_to(package / "dal"):
            raise ValueError(f"DAL import escaped the selected build: {path}")
    from dal_benchmarks.cases import build_cases
    from dal_benchmarks.runner import main as run_benchmarks

    if args.inventory:
        args.output.mkdir(parents=True, exist_ok=True)
        manifest = [case.description() for case in build_cases(smoke=False)]
        (args.output / "inventory.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
        )
        return 0
    return run_benchmarks(
        ["--samples", "1", "--warmups", "2", "--output-dir", str(args.output)]
    )


if __name__ == "__main__":
    raise SystemExit(main())

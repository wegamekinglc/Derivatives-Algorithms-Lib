#!/usr/bin/env python3
"""The fixed eight-case workload prefix preceding DAL-469's Python failure."""

import argparse
from importlib.machinery import EXTENSION_SUFFIXES
from pathlib import Path
import sys


PREFIX = (
    "rng.sobol_normal_fast", "rng.sobol_normal_precise", "rng.sobol_uniform", "rng.mrg32_normal",
    "script.construct_and_parse", "mc.vanilla.double.default", "mc.vanilla.double.tree", "mc.vanilla.double.compiled",
)


def select_prefix(cases):
    selected = cases[:len(PREFIX)]
    if tuple(case.name for case in selected) != PREFIX:
        raise ValueError("head workload prefix changed")
    return selected


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("suite", "package", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args(argv)
    if sys.platform == "linux":
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    package = args.package.resolve()
    sys.path[:0] = [str(package), str(args.suite.resolve())]
    try:
        import dal
    except (ImportError, OSError) as error:
        print(f"Original/native package cannot be imported: {error}", file=sys.stderr)
        return 78
    for path in (dal.__file__, dal._dal.__file__):
        if not Path(path).resolve().is_relative_to(package / "dal"):
            raise ValueError(f"DAL import escaped selected package: {path}")
    if not any(str(dal._dal.__file__).endswith(suffix) for suffix in EXTENSION_SUFFIXES):
        raise ValueError("prefix requires the compiled native extension")
    from dal_benchmarks.cases import build_cases
    from dal_benchmarks.runner import run_cases
    return run_cases(select_prefix(build_cases(smoke=False)), argparse.Namespace(
        smoke=False, samples=1, warmups=2, output_dir=args.output,
    ))


if __name__ == "__main__":
    raise SystemExit(main())

"""Direct C++/.NET discount-query comparison; Python never enters native timers."""

import argparse
from pathlib import Path

from dal_benchmarks.harness import require
from . import infine

SCHEMA = "dal.native-query-comparison/1"
FIXTURE_SCHEMA = "dal.native-query-fixture/1"
WORKER_SCHEMA = "dal.native-query-worker/1"
BACKENDS = ("dal", "quantlib", "infine")
QUANTLIB_REVISION = "6b57206e04598f092efee66e3b367efc84771995"


def fixture(shared=None):
    shared = shared if shared is not None else infine.fixtures(False, ["discount_queries"])
    require(len(shared["cases"]) == 1, "native comparison requires one case")
    case = shared["cases"][0]
    require(case["workload"]["name"] == "discount_queries", "wrong native query case")
    tolerances = set(case["tolerance"])
    require(tolerances == {2e-12}, "native query tolerance changed")
    return {
        "schema": FIXTURE_SCHEMA,
        "workload": case["workload"],
        "anchor": shared["rates"]["today"],
        "dates": [shared["rates"]["today"], *shared["rates"]["dates"]],
        "dfs": [1.0, *shared["rates"]["dfs"]],
        "queries": case["queries"],
        "reference": case["reference"],
        "absolute_tolerance": 2e-12,
        "relative_tolerance": 1e-10,
    }


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-worker", type=Path, required=True)
    parser.add_argument("--native-build-root", type=Path, required=True)
    parser.add_argument("--infine-build", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=10)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args(argv)
    args.native_worker = args.native_worker.resolve()
    args.native_build_root = args.native_build_root.resolve()
    args.infine_build = args.infine_build.resolve()
    args.output_dir = args.output_dir.resolve()
    require(args.samples >= 10 and args.rounds >= 2, "native comparison needs >=10 samples and >=2 rounds")
    require(args.timeout > 0, "timeout must be positive")
    require(args.native_worker.is_file(), "native query worker missing")
    require((args.native_build_root / "CMakeCache.txt").is_file(), "native CMake cache missing")
    require(args.native_worker.is_relative_to(args.native_build_root), "worker escaped native build root")
    require(args.infine_build.is_dir(), "InFine build directory missing")
    return args

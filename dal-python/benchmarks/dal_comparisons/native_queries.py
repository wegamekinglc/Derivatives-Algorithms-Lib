"""Direct C++/.NET discount-query comparison; Python never enters native timers."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess  # nosec B404
import sys
from types import SimpleNamespace

from dal_benchmarks.harness import require
from . import infine, scenarios
from .evidence import sha256, write_json

SCHEMA = "dal.native-query-comparison/1"
FIXTURE_SCHEMA = "dal.native-query-fixture/1"
WORKER_SCHEMA = "dal.native-query-worker/1"
BACKENDS = ("dal", "quantlib", "infine")
METHODS = {
    "dal": "DAL C++ DiscountLogDF",
    "quantlib": "QuantLib C++ DiscountCurve",
    "infine": "InFine C# InterpolatedDiscountCurve",
}
QUANTLIB_REVISION = "6b57206e04598f092efee66e3b367efc84771995"
ROOT = Path(__file__).resolve().parents[1]
REPO = Path(__file__).resolve().parents[3]


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


def native_source_hashes():
    paths = [
        ROOT / "run_native_queries.py",
        ROOT / "dal_comparisons/native_queries.py",
        ROOT / "dal_comparisons/scenarios.py",
        ROOT / "dal_comparisons/infine.py",
        ROOT / "dal_comparisons/evidence.py",
        REPO / "dal-cpp/benchmarks/native_query_comparison/queryworker.cpp",
        REPO / "dal-cpp/benchmarks/native_query_comparison/CMakeLists.txt",
        REPO / "dal-cpp/benchmarks/CMakeLists.txt",
        REPO / "dal-cpp/CMakeLists.txt",
    ]
    return {path.relative_to(REPO).as_posix(): sha256(path) for path in sorted(paths)}


def backend_order(sample, round_index):
    offset = sample % len(BACKENDS)
    backends = list(BACKENDS[offset:] + BACKENDS[:offset])
    return backends[::-1] if round_index % 2 else backends


def check_worker(row, backend, payload):
    require(row["schema"] == WORKER_SCHEMA, "unexpected native worker schema")
    require(row["backend"] == backend, "wrong native worker backend")
    require(row["workload"] == payload["workload"], "native workload changed")
    require(row["method"] == METHODS[backend], "native query method changed")
    durations = row["samples_ns"]
    require(len(durations) == 1, "native worker must return one sample")
    require(type(durations[0]) is int and durations[0] > 0, "invalid native timing")
    scenarios.validate(
        row["values"], payload["reference"], abs_tol=payload["absolute_tolerance"]
    )


def invoke_cpp(args, backend, directory, payload):
    before = sha256(args.native_worker)
    source = directory / "native-fixture.json"
    output = directory / "worker.json"
    write_json(source, payload)
    command = [
        str(args.native_worker),
        "--backend",
        backend,
        "--input",
        str(source),
        "--output",
        str(output),
    ]
    environment = dict(
        os.environ,
        DAL_NUM_THREADS="1",
        OMP_NUM_THREADS="1",
        OPENBLAS_NUM_THREADS="1",
        MKL_NUM_THREADS="1",
    )
    with (directory / "worker.log").open("w", encoding="utf-8") as log:
        # Literal executable and paths; the native timer excludes this process boundary.
        subprocess.run(  # nosec B603  # nosemgrep
            command,
            shell=False,
            check=True,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=args.timeout,
            env=environment,
        )
    require(
        sha256(args.native_worker) == before,
        "native worker changed during execution",
    )
    row = json.loads(output.read_text(encoding="utf-8"))
    check_worker(row, backend, payload)
    require(row.get("quantlib_version") == "1.43", "native QuantLib version changed")
    require(bool(row.get("boost_version")), "native Boost version missing")
    row["identity"] = {
        "worker_sha256": before,
        "quantlib_version": row["quantlib_version"],
        "boost_version": row["boost_version"],
    }
    return row


def invoke_infine(args, directory, shared):
    worker_args = SimpleNamespace(
        infine_build=args.infine_build,
        output_dir=directory,
        smoke=False,
        case=["discount_queries"],
        timeout=args.timeout,
    )
    rows, files, metadata = infine.measure_cases(worker_args, shared)
    require(len(rows) == 1, "InFine native query case count changed")
    native = rows[0]
    row = {
        "schema": WORKER_SCHEMA,
        "backend": "infine",
        "workload": native["workload"],
        "method": METHODS["infine"],
        "samples_ns": native["samples_ns"],
        "values": native["values"],
        "native_method": native["method"],
        "identity": {"files": files, "version": metadata},
    }
    check_worker(row, "infine", fixture(shared))
    return row


def cmake_identity(build_root):
    settings = {}
    for line in (build_root / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("//") or ":" not in line or "=" not in line:
            continue
        key = line.split(":", 1)[0]
        settings[key] = line.split("=", 1)[1]
    require(settings.get("CMAKE_BUILD_TYPE") == "Release", "native comparison requires Release")
    require(settings.get("DAL_CPP_BUILD_BENCHMARKS") == "ON", "native benchmarks are disabled")
    require(
        settings.get("DAL_CPP_BUILD_NATIVE_COMPARISONS") == "ON",
        "native comparisons are disabled",
    )
    require("QuantLib_DIR" in settings, "native QuantLib package path missing")
    names = (
        "CMAKE_BUILD_TYPE",
        "CMAKE_CXX_COMPILER",
        "CMAKE_GENERATOR",
        "CMAKE_CXX_STANDARD",
        "BUILD_SHARED_LIBS",
        "DAL_CPP_BUILD_BENCHMARKS",
        "DAL_CPP_BUILD_NATIVE_COMPARISONS",
        "DAL_ENABLE_NATIVE_ARCH",
        "DAL_BUILD_PUBLIC",
        "DAL_BUILD_PYTHON",
        "DAL_BUILD_EXCEL",
        "QuantLib_DIR",
    )
    return {name: settings[name] for name in names if name in settings}


def git_identity():
    try:
        marker = REPO / ".git"
        if marker.is_dir():
            prefix = ["git", "-C", str(REPO)]
        else:
            declaration = marker.read_text(encoding="utf-8").strip()
            require(declaration.startswith("gitdir:"), "invalid git worktree marker")
            git_dir = declaration.split(":", 1)[1].strip()
            if sys.platform != "win32" and len(git_dir) > 1 and git_dir[1] == ":":
                git_dir = subprocess.check_output(  # nosec B603  # nosemgrep
                    ["wslpath", git_dir], text=True, stderr=subprocess.STDOUT, shell=False
                ).strip()
            prefix = [f"--git-dir={git_dir}", f"--work-tree={REPO}"]
        revision = subprocess.check_output(  # nosec B603  # nosemgrep
            ["git", *prefix, "rev-parse", "HEAD"],
            text=True,
            stderr=subprocess.STDOUT,
            shell=False,
        ).strip()
        status = subprocess.check_output(  # nosec B603  # nosemgrep
            ["git", *prefix, "status", "--porcelain"],
            text=True,
            stderr=subprocess.STDOUT,
            shell=False,
        ).strip()
        return {"revision": revision, "dirty": bool(status), "status": status}
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        return {"revision": "unavailable", "error": f"{type(error).__name__}: {error}"}


def aggregate(reports):
    measurements = {}
    for backend in BACKENDS:
        rows = reports[backend]
        require(bool(rows), f"missing native samples for {backend}")
        durations = [duration for row in rows for duration in row["samples_ns"]]
        measurements[backend] = {
            "samples_ns": durations,
            "min_ns": min(durations),
            "median_ns": statistics.median(durations),
            "method": rows[0]["method"],
            "values": rows[0]["values"],
        }
    dal_time = measurements["dal"]["min_ns"]
    ratios = {
        backend: measurements[backend]["min_ns"] / dal_time
        for backend in BACKENDS[1:]
    }
    return {
        "workload": reports["dal"][0]["workload"],
        "backends": measurements,
        "third_party_over_dal": ratios,
    }


def summary(report):
    lines = [
        "# Native C++/.NET discount-query comparison",
        "",
        f"Status: {report['status']}.",
        "",
        "DAL C++ and QuantLib C++ call native curves directly; InFine C# calls its native .NET curve directly. Python/JSON/IPC/startup are outside timing.",
        "",
        "Each process performs three checked warmups, then one native sample. Curves and query containers are prepared before timing; native array/vector creation is included. Validation and serialization are outside timing.",
        "",
        "Ratios are third-party minimum / DAL minimum and are informational, not a performance gate. C++ and .NET remain different runtimes.",
        "",
    ]
    if report["status"] != "passed":
        lines += [f"Error: {report.get('error', 'unknown error')}", ""]
        return "\n".join(lines) + "\n"
    lines += [
        "| Round | Case | DAL C++ min ms | QuantLib C++ min ms | InFine C# min ms | QL / DAL | InFine / DAL |",
        "| ----- | ---- | -------------- | ------------------- | ---------------- | -------- | ------------ |",
    ]
    for index, result in enumerate(report["rounds"], 1):
        for row in result["cases"]:
            values = row["backends"]
            times = " | ".join(f"{values[backend]['min_ns'] / 1e6:.6f}" for backend in BACKENDS)
            ratios = row["third_party_over_dal"]
            lines.append(
                f"| {index} | {row['workload']['name']} | {times} | "
                + " | ".join(f"{ratios[backend]:.6g}" for backend in BACKENDS[1:])
                + " |"
            )
    return "\n".join(lines) + "\n"


def run(args):
    args.output_dir.mkdir(parents=True, exist_ok=False)
    report = {
        "schema": SCHEMA,
        "status": "running",
        "samples_per_round": args.samples,
        "warmups": 3,
        "backends": list(BACKENDS),
        "identities": {},
        "rounds": [],
    }
    try:
        shared = infine.fixtures(False, ["discount_queries"])
        payload = fixture(shared)
        report.update(
            workload=payload["workload"],
            fixture_sha256=hashlib.sha256(
                json.dumps(payload, sort_keys=True).encode("utf-8")
            ).hexdigest(),
            source_hashes=native_source_hashes(),
            native_worker_sha256=sha256(args.native_worker),
            cmake=cmake_identity(args.native_build_root),
            git=git_identity(),
            thread_environment={
                "DAL_NUM_THREADS": "1",
                "OMP_NUM_THREADS": "1",
                "OPENBLAS_NUM_THREADS": "1",
                "MKL_NUM_THREADS": "1",
            },
        )
        for round_index in range(args.rounds):
            reports = {backend: [] for backend in BACKENDS}
            order = []
            for sample in range(args.samples):
                backends = backend_order(sample, round_index)
                order.append(backends)
                for backend in backends:
                    directory = (
                        args.output_dir
                        / f"round-{round_index + 1}"
                        / f"sample-{sample + 1}-{backend}"
                    )
                    directory.mkdir(parents=True, exist_ok=False)
                    row = (
                        invoke_infine(args, directory, shared)
                        if backend == "infine"
                        else invoke_cpp(args, backend, directory, payload)
                    )
                    identity = row.pop("identity")
                    require(
                        report["identities"].setdefault(backend, identity) == identity,
                        f"native provenance changed for {backend}",
                    )
                    reports[backend].append(row)
            report["rounds"].append(
                {"order": order, "cases": [aggregate(reports)]}
            )
        require(native_source_hashes() == report["source_hashes"], "native source changed during run")
        require(
            sha256(args.native_worker) == report["native_worker_sha256"],
            "native worker changed during run",
        )
        for backend in BACKENDS:
            count = sum(
                len(result["cases"][0]["backends"][backend]["samples_ns"])
                for result in report["rounds"]
            )
            require(count == args.samples * args.rounds, f"incomplete native samples for {backend}")
        report["status"] = "passed"
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        report["status"] = "failed"
        report["error"] = f"{type(error).__name__}: {error}"
    write_json(args.output_dir / "results.json", report)
    markdown = summary(report)
    (args.output_dir / "summary.md").write_text(markdown, encoding="utf-8")
    print(markdown)
    return 0 if report["status"] == "passed" else 1


def main(argv=None):
    return run(arguments(argv))

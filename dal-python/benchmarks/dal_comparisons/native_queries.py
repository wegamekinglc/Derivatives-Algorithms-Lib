"""Direct C++/.NET curve comparisons; Python never enters native timers."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess  # nosec B404
import sys
from types import SimpleNamespace

from dal_benchmarks.harness import require
from . import calibration_scenarios, infine, scenarios
from .evidence import sha256, write_json

SCHEMA = "dal.native-query-comparison/1"
FIXTURE_SCHEMA = "dal.native-query-fixture/1"
WORKER_SCHEMA = "dal.native-query-worker/1"
BACKENDS = ("dal", "quantlib", "infine")
CASES = (
    "discount_queries",
    "calibration_single_5",
    "calibration_single_15",
    "calibration_multi_staged_5",
    "calibration_multi_staged_15",
)
CALIBRATION_CASES = {
    "calibration_single_5": ("single", 5),
    "calibration_single_15": ("single", 15),
    "calibration_multi_staged_5": ("multi_staged", 5),
    "calibration_multi_staged_15": ("multi_staged", 15),
}
SCHEMAS = {CASES[0]: SCHEMA, **{case: "dal.native-calibration-comparison/1" for case in CALIBRATION_CASES}}
FIXTURE_SCHEMAS = {CASES[0]: FIXTURE_SCHEMA, **{case: "dal.native-calibration-fixture/1" for case in CALIBRATION_CASES}}
WORKER_SCHEMAS = {CASES[0]: WORKER_SCHEMA, **{case: "dal.native-calibration-worker/1" for case in CALIBRATION_CASES}}


def calibration_methods(kind):
    structure = "single-curve" if kind == "single" else "staged two-curve"
    return {
        "dal": f"DAL C++ analytic {structure} solve (tol 1e-12, fit 1e-10, diagnostics off)",
        "quantlib": "QuantLib C++ PiecewiseYieldCurve<Discount, LogLinear> (accuracy 1e-12)",
        "infine": "InFine C# native Brent log-linear bootstrap (tolerance 1e-12)",
    }


METHODS = {
    "discount_queries": {
        "dal": "DAL C++ DiscountLogDF",
        "quantlib": "QuantLib C++ DiscountCurve",
        "infine": "InFine C# InterpolatedDiscountCurve",
    },
    **{case: calibration_methods(kind) for case, (kind, _) in CALIBRATION_CASES.items()},
}
QUANTLIB_REVISION = "6b57206e04598f092efee66e3b367efc84771995"
ROOT = Path(__file__).resolve().parents[1]
REPO = Path(__file__).resolve().parents[3]


def fixture(shared=None, case_name="discount_queries"):
    require(case_name in CASES, "unsupported native comparison case")
    shared = shared if shared is not None else infine.fixtures(False, [case_name])
    require(len(shared["cases"]) == 1, "native comparison requires one case")
    case = shared["cases"][0]
    require(case["workload"]["name"] == case_name, "wrong native comparison case")
    tolerances = set(case["tolerance"])
    if case_name in CALIBRATION_CASES:
        kind, size = CALIBRATION_CASES[case_name]
        require(tolerances == {2e-8}, "native calibration tolerance changed")
        result = {
            "schema": FIXTURE_SCHEMAS[case_name],
            "workload": case["workload"],
            "anchor": shared["rates"]["today"],
            "dates": [value.isoformat() for value in calibration_scenarios.dates(size)],
            "queries": case["queries"],
            "quotes": case["quotes"]["usd_discount"],
            "reference": case["reference"],
            "absolute_tolerance": 2e-8,
            "relative_tolerance": 1e-10,
        }
        if kind == "multi_staged":
            result["forward_quotes"] = case["quotes"]["usd_forward"]
        return result
    require(tolerances == {2e-12}, "native query tolerance changed")
    return {
        "schema": FIXTURE_SCHEMAS[case_name],
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
    parser.add_argument("--quantlib-source", type=Path, required=True)
    parser.add_argument("--infine-build", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--case", action="append", choices=CASES, default=[])
    parser.add_argument("--samples", type=int, default=10)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args(argv)
    args.native_worker = args.native_worker.resolve()
    args.native_build_root = args.native_build_root.resolve()
    args.quantlib_source = args.quantlib_source.resolve()
    args.infine_build = args.infine_build.resolve()
    args.output_dir = args.output_dir.resolve()
    require(args.samples >= 10 and args.rounds >= 2, "native comparison needs >=10 samples and >=2 rounds")
    require(args.timeout > 0, "timeout must be positive")
    require(args.native_worker.is_file(), "native query worker missing")
    require((args.native_build_root / "CMakeCache.txt").is_file(), "native CMake cache missing")
    require(args.native_worker.is_relative_to(args.native_build_root), "worker escaped native build root")
    require(args.quantlib_source.is_dir(), "QuantLib source directory missing")
    require(args.infine_build.is_dir(), "InFine build directory missing")
    selected_cases(args)
    return args


def selected_cases(args):
    raw = getattr(args, "case", [])
    if isinstance(raw, str):
        raw = [raw]
    selected = list(raw) if raw else ["discount_queries"]
    require(all(case in CASES for case in selected), "unsupported native comparison case")
    require(len(set(selected)) == len(selected), "duplicate comparison case")
    return selected


def native_source_hashes():
    paths = [
        ROOT / "run_native_queries.py",
        ROOT / "dal_comparisons/native_queries.py",
        ROOT / "dal_comparisons/scenarios.py",
        ROOT / "dal_comparisons/calibration_scenarios.py",
        ROOT / "dal_comparisons/infine.py",
        ROOT / "dal_comparisons/evidence.py",
        ROOT / "infine/Rates.cs",
        ROOT / "infine/Program.cs",
        REPO / "dal-cpp/dal/curve/yclogdf.cpp",
        REPO / "dal-cpp/dal/curve/calibration.cpp",
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
    case_name = payload["workload"]["name"]
    require(row["schema"] == WORKER_SCHEMAS[case_name], "unexpected native worker schema")
    require(row["backend"] == backend, "wrong native worker backend")
    require(row["workload"] == payload["workload"], "native workload changed")
    require(row["method"] == METHODS[case_name][backend], "native query method changed")
    durations = row["samples_ns"]
    require(len(durations) == 1, "native worker must return one sample")
    require(type(durations[0]) is int and durations[0] > 0, "invalid native timing")
    scenarios.validate(
        row["values"], payload["reference"], abs_tol=payload["absolute_tolerance"]
    )


def invoke_cpp(args, backend, directory, payload, case_name=None):
    before = sha256(args.native_worker)
    source = directory / "native-fixture.json"
    output = directory / "worker.json"
    write_json(source, payload)
    case_name = case_name or getattr(args, "case", "discount_queries")
    if isinstance(case_name, list):
        case_name = case_name[0] if case_name else "discount_queries"
    command = [
        str(args.native_worker),
        "--backend",
        backend,
        "--input",
        str(source),
        "--output",
        str(output),
        "--case",
        case_name,
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


def invoke_infine(args, directory, shared, case_name=None):
    case_name = case_name or getattr(args, "case", "discount_queries")
    if isinstance(case_name, list):
        case_name = case_name[0] if case_name else "discount_queries"
    worker_args = SimpleNamespace(
        infine_build=args.infine_build,
        output_dir=directory,
        smoke=False,
        case=[case_name],
        timeout=args.timeout,
    )
    rows, files, metadata = infine.measure_cases(worker_args, shared)
    require(len(rows) == 1, "InFine native query case count changed")
    native = rows[0]
    row = {
        "schema": WORKER_SCHEMAS[case_name],
        "backend": "infine",
        "workload": native["workload"],
        "method": METHODS[case_name]["infine"],
        "samples_ns": native["samples_ns"],
        "values": native["values"],
        "native_method": native["method"],
        "identity": {"files": files, "version": metadata},
    }
    check_worker(row, "infine", fixture(shared, case_name))
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
    require("CMAKE_HOME_DIRECTORY" in settings, "native CMake source directory missing")
    require(
        Path(settings["CMAKE_HOME_DIRECTORY"]).resolve() == REPO,
        "native build source does not match current DAL checkout",
    )
    names = (
        "CMAKE_BUILD_TYPE",
        "CMAKE_HOME_DIRECTORY",
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


def quantlib_identity(source):
    git = shutil.which("git")
    require(git is not None, "git executable missing")
    prefix = [git, "-C", str(Path(source).resolve())]
    revision = subprocess.check_output(  # nosec B603  # nosemgrep
        [*prefix, "rev-parse", "HEAD"],
        text=True,
        stderr=subprocess.STDOUT,
        shell=False,
    ).strip()
    require(revision == QUANTLIB_REVISION, "native QuantLib source revision changed")
    status = subprocess.check_output(  # nosec B603  # nosemgrep
        [*prefix, "status", "--porcelain", "--ignore-submodules=dirty"],
        text=True,
        stderr=subprocess.STDOUT,
        shell=False,
    ).strip()
    require(not status, "native QuantLib source is not clean")
    return {"revision": revision, "dirty": False}


def git_identity():
    try:
        git = shutil.which("git")
        require(git is not None, "git executable missing")
        marker = REPO / ".git"
        if marker.is_dir():
            prefix = ["-C", str(REPO)]
        else:
            declaration = marker.read_text(encoding="utf-8").strip()
            require(declaration.startswith("gitdir:"), "invalid git worktree marker")
            git_dir = declaration.split(":", 1)[1].strip()
            if sys.platform != "win32" and len(git_dir) > 1 and git_dir[1] == ":":
                wslpath = shutil.which("wslpath")
                require(wslpath is not None, "wslpath executable missing")
                git_dir = subprocess.check_output(  # nosec B603  # nosemgrep
                    [wslpath, git_dir], text=True, stderr=subprocess.STDOUT, shell=False
                ).strip()
            prefix = [f"--git-dir={git_dir}", f"--work-tree={REPO}"]
        revision = subprocess.check_output(  # nosec B603  # nosemgrep
            [git, *prefix, "rev-parse", "HEAD"],
            text=True,
            stderr=subprocess.STDOUT,
            shell=False,
        ).strip()
        status = subprocess.check_output(  # nosec B603  # nosemgrep
            [git, *prefix, "status", "--porcelain", "--ignore-submodules=dirty"],
            text=True,
            stderr=subprocess.STDOUT,
            shell=False,
        ).strip()
        return {
            "revision": revision,
            "dirty": bool(status),
            "status": status,
            "submodule_policy": "ignore dirty worktrees",
        }
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        return {"revision": "unavailable", "error": f"{type(error).__name__}: {error}"}


def clean_git_identity():
    identity = git_identity()
    require(identity.get("revision") != "unavailable", "native DAL Git identity unavailable")
    require(not identity.get("dirty", True), "native DAL source is not clean")
    return identity


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


def summary_style(report):
    suite = len(report.get("case", [])) > 1
    case_name = report.get("workload", {}).get("name", "discount_queries")
    calibration = case_name in CALIBRATION_CASES and not suite
    if suite:
        title = "# Native C++/.NET curve suite"
        boundary = "DAL C++, QuantLib C++, and InFine C# run native discount and single/staged calibration cases independently. Python/JSON/IPC/startup are outside timing."
        measurement = "Each process performs three checked warmups, then one native sample with complete per-case evidence. Discount queries use prebuilt curves and include query evaluation and native output allocation; calibration cases include fresh instruments, initial curves, solves, DF conversion, and native allocation."
    else:
        title = "# Native C++/.NET curve-calibration comparison" if calibration else "# Native C++/.NET discount-query comparison"
        boundary = (
            "DAL C++ and QuantLib C++ construct and bootstrap native instruments directly; InFine C# builds and solves native .NET instruments. Python/JSON/IPC/startup are outside timing."
            if calibration
            else "DAL C++ and QuantLib C++ call native curves directly; InFine C# calls its native .NET curve directly. Python/JSON/IPC/startup are outside timing."
        )
        kind, size = CALIBRATION_CASES.get(case_name, (None, None))
        output_width = (2 if kind == "single" else 4) * size if calibration else 4096
        measurement = (
            f"Each process performs three checked warmups, then one native sample. Fresh instrument construction, initial curves, solving, {output_width} DF conversions, and native array/vector creation are included. Raw quotes, reference values, validation, and serialization are outside timing."
            if calibration
            else "Each process performs three checked warmups, then one native sample. Curves and query containers are prepared before timing; native array/vector creation is included. Validation and serialization are outside timing."
        )
    return [
        title,
        "",
        f"Status: {report['status']}.",
        "",
        boundary,
        "",
        measurement,
        "",
        "Ratios are third-party minimum / DAL minimum and are informational, not a performance gate. C++ and .NET remain different runtimes.",
        "",
    ]


def summary_rows(report):
    lines = [
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
    return lines


def summary(report):
    lines = summary_style(report)
    if report["status"] != "passed":
        lines += [f"Error: {report.get('error', 'unknown error')}", ""]
        return "\n".join(lines) + "\n"
    return "\n".join(lines + summary_rows(report)) + "\n"


def collect_round(args, case_name, shared, payload, identities, round_index):
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
                invoke_infine(args, directory, shared, case_name)
                if backend == "infine"
                else invoke_cpp(args, backend, directory, payload, case_name)
            )
            identity = row.pop("identity")
            require(
                identities.setdefault(backend, identity) == identity,
                f"native provenance changed for {backend}",
            )
            reports[backend].append(row)
    return {"order": order, "workers": reports, "cases": [aggregate(reports)]}


def verify_provenance(args, report):
    require(
        cmake_identity(args.native_build_root) == report["cmake"],
        "native CMake configuration changed during run",
    )
    require(native_source_hashes() == report["source_hashes"], "native source changed during run")
    require(
        sha256(args.native_worker) == report["native_worker_sha256"],
        "native worker changed during run",
    )
    require(
        quantlib_identity(args.quantlib_source) == report["quantlib"],
        "native QuantLib provenance changed during run",
    )
    require(
        clean_git_identity() == report["git"],
        "native DAL Git provenance changed during run",
    )


def verify_single(args, report):
    verify_provenance(args, report)
    for backend in BACKENDS:
        count = sum(
            len(result["cases"][0]["backends"][backend]["samples_ns"])
            for result in report["rounds"]
        )
        require(count == args.samples * args.rounds, f"incomplete native samples for {backend}")


def run_single(args, case_name):
    args.output_dir.mkdir(parents=True, exist_ok=False)
    report = {
        "schema": SCHEMAS[case_name],
        "status": "running",
        "case": [case_name],
        "samples_per_round": args.samples,
        "warmups": 3,
        "backends": list(BACKENDS),
        "identities": {},
        "rounds": [],
    }
    try:
        shared = infine.fixtures(False, [case_name])
        payload = fixture(shared, case_name)
        report.update(
            workload=payload["workload"],
            fixture_sha256=hashlib.sha256(
                json.dumps(payload, sort_keys=True).encode("utf-8")
            ).hexdigest(),
            source_hashes=native_source_hashes(),
            native_worker_sha256=sha256(args.native_worker),
            cmake=cmake_identity(args.native_build_root),
            quantlib=quantlib_identity(args.quantlib_source),
            git=clean_git_identity(),
            thread_environment={
                "DAL_NUM_THREADS": "1",
                "OMP_NUM_THREADS": "1",
                "OPENBLAS_NUM_THREADS": "1",
                "MKL_NUM_THREADS": "1",
            },
        )
        for round_index in range(args.rounds):
            report["rounds"].append(
                collect_round(
                    args, case_name, shared, payload, report["identities"], round_index
                )
            )
        verify_single(args, report)
        report["status"] = "passed"
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        report["status"] = "failed"
        report["error"] = f"{type(error).__name__}: {error}"
    write_json(args.output_dir / "results.json", report)
    markdown = summary(report)
    (args.output_dir / "summary.md").write_text(markdown, encoding="utf-8")
    print(markdown)
    return 0 if report["status"] == "passed" else 1


def merge_child_provenance(report, child, case_name):
    for key in ("source_hashes", "native_worker_sha256", "cmake", "quantlib", "git", "thread_environment"):
        require(
            report.setdefault(key, child[key]) == child[key],
            f"native provenance changed for {case_name}",
        )
    for backend, identity in child["identities"].items():
        require(
            report["identities"].setdefault(backend, identity) == identity,
            f"native provenance changed for {backend}",
        )
    report["fixture_sha256"][case_name] = child["fixture_sha256"]


def merge_child_rounds(report, child, case_name, payload):
    for round_index, child_round in enumerate(child["rounds"]):
        if len(report["rounds"]) <= round_index:
            report["rounds"].append({"order": {}, "workers": {}, "cases": []})
        target = report["rounds"][round_index]
        target["order"][case_name] = child_round["order"]
        target["workers"][case_name] = child_round["workers"]
        target["cases"].extend(child_round["cases"])
        for backend, rows in child_round["workers"].items():
            for row in rows:
                check_worker(row, backend, payload)


def merge_child_report(report, child, case_name):
    merge_child_provenance(report, child, case_name)
    merge_child_rounds(report, child, case_name, fixture(case_name=case_name))


def check_suite_samples(args, report, case_names):
    for case_name in case_names:
        for backend in BACKENDS:
            count = sum(
                len(result["workers"][case_name][backend])
                for result in report["rounds"]
            )
            require(
                count == args.samples * args.rounds,
                f"incomplete native samples for {case_name}/{backend}",
            )


def run_suite(args, case_names):
    args.output_dir.mkdir(parents=True, exist_ok=False)
    report = {
        "schema": "dal.native-curve-comparison/1",
        "status": "running",
        "case": list(case_names),
        "workload": {"name": "native_curve_suite"},
        "samples_per_round": args.samples,
        "warmups": 3,
        "backends": list(BACKENDS),
        "fixture_sha256": {},
        "identities": {},
        "rounds": [],
    }
    try:
        for case_name in case_names:
            case_args = SimpleNamespace(**vars(args))
            case_args.output_dir = args.output_dir / case_name
            case_args.case = case_name
            require(run_single(case_args, case_name) == 0, f"native case failed: {case_name}")
            child = json.loads((case_args.output_dir / "results.json").read_text(encoding="utf-8"))
            require(child["status"] == "passed", f"native case failed: {case_name}")
            require(len(child["rounds"]) == args.rounds, f"incomplete rounds for {case_name}")
            merge_child_report(report, child, case_name)
        check_suite_samples(args, report, case_names)
        verify_provenance(args, report)
        report["status"] = "passed"
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        report["status"] = "failed"
        report["error"] = f"{type(error).__name__}: {error}"
    write_json(args.output_dir / "results.json", report)
    markdown = summary(report)
    (args.output_dir / "summary.md").write_text(markdown, encoding="utf-8")
    print(markdown)
    return 0 if report["status"] == "passed" else 1


def run(args):
    case_names = selected_cases(args)
    if len(case_names) > 1:
        return run_suite(args, case_names)
    return run_single(args, case_names[0])


def main(argv=None):
    return run(arguments(argv))

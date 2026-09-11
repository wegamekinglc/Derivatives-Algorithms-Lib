"""Command-line runner: retain all samples, failures and environment provenance."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess  # nosec B404
import sys
import time

from .cases import CPP_COVERAGE, build_cases
from .harness import measure, select_cases


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git_value(root, *args):
    if args not in (("rev-parse", "HEAD"), ("status", "--porcelain")):
        raise ValueError("unsupported Git metadata query")
    executable = shutil.which("git")
    if executable is None:
        return None
    try:
        # Resolve the fixed tool, allow only the two read-only queries above, and
        # pass the checkout path literally without shell expansion.
        return subprocess.run(  # nosec B603  # nosemgrep
            [str(Path(executable).resolve()), "-C", str(root), *args],
            shell=False,
            check=True,
            capture_output=True,
            text=True,
            timeout=10,
        ).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return None


def cmake_environment(native):
    cache = next(
        (
            parent / "CMakeCache.txt"
            for parent in native.parents
            if (parent / "CMakeCache.txt").is_file()
        ),
        None,
    )
    cmake = {}
    if cache:
        for line in cache.read_text(encoding="utf-8").splitlines():
            if line.startswith(
                (
                    "CMAKE_BUILD_TYPE:",
                    "CMAKE_CXX_COMPILER:",
                    "CMAKE_CXX_FLAGS",
                    "DAL_USE_",
                    "DAL_ENABLE_NATIVE_ARCH:",
                )
            ):
                key, value = line.split("=", 1)
                cmake[key] = value
    return str(cache) if cache else None, cmake


def cpu_model():
    cpu = platform.processor()
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        cpu = next(
            (
                line.split(":", 1)[1].strip()
                for line in cpuinfo.read_text().splitlines()
                if line.startswith("model name")
            ),
            cpu,
        )
    return cpu


def environment():
    import dal

    root = Path(__file__).resolve().parents[3]
    native = Path(dal._dal.__file__).resolve()
    cache, cmake = cmake_environment(native)
    suite = Path(__file__).resolve().parent.parent
    paths = list((suite / "dal_benchmarks").glob("*.py"))
    paths += list((suite / "dal_comparisons").glob("*.py"))
    sources = {
        path.relative_to(suite).as_posix(): sha256(path) for path in sorted(paths)
    }
    return {
        "utc": datetime.now(timezone.utc).isoformat(),
        "python": sys.version,
        "executable": sys.executable,
        "platform": platform.platform(),
        "machine": platform.machine(),
        "cpu": cpu_model(),
        "cpu_count": os.cpu_count(),
        "dal_version": dal.__version__,
        "dal_package": str(Path(dal.__file__).resolve()),
        "native_module": str(native),
        "native_sha256": sha256(native),
        "cmake_cache": cache,
        "cmake": cmake,
        "source_head": git_value(root, "rev-parse", "HEAD"),
        "source_status": git_value(root, "status", "--porcelain"),
        "suite_sha256": sources,
        "thread_environment": {
            key: os.environ.get(key)
            for key in (
                "DAL_NUM_THREADS",
                "OMP_NUM_THREADS",
                "OPENBLAS_NUM_THREADS",
                "MKL_NUM_THREADS",
            )
        },
        "clock": vars(time.get_clock_info("perf_counter")),
        "provenance_note": "Source HEAD describes this checkout; native SHA256 identifies the loaded binary. An installed wheel may come from a different revision.",
    }


def write_report(output, report):
    output.mkdir(parents=True, exist_ok=True)
    (output / "results.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )
    lines = [
        "# DAL Python interface benchmarks",
        "",
        f"Profile: {report['profile']}; status: {report['status']}.",
        "",
        "Informational in-process timings, including Python/native conversion. No C++ speed ratio or regression verdict is inferred.",
        "",
        f"Samples per case: {report['samples']}; warmups: {report['warmups']}; reduction: minimum (median also shown).",
        "",
    ]
    environment_data = report["environment"]
    for key in (
        "source_head",
        "dal_version",
        "native_module",
        "native_sha256",
        "platform",
        "cpu",
    ):
        if key in environment_data:
            lines.append(f"- {key}: `{environment_data[key]}`")
    lines += [
        "",
        "| Case | C++ target | Min (ms) | Median (ms) | Status |",
        "|---|---|---:|---:|---|",
    ]
    for row in report["results"]:
        if row["status"] == "passed":
            lines.append(
                f"| {row['name']} | {row['cpp_target']} | {row['min_ns'] / 1e6:.6f} | {row['median_ns'] / 1e6:.6f} | passed |"
            )
        else:
            error = row["error"].replace("\n", " ").replace("|", "\\|")
            lines.append(
                f"| {row['name']} | {row['cpp_target']} | — | — | FAILED: {error} |"
            )
    lines += [
        "",
        "## Native coverage",
        "",
        "| Target | Coverage | Boundary / gap |",
        "|---|---|---|",
    ]
    for target, entry in CPP_COVERAGE.items():
        lines.append(f"| {target} | {entry['status']} | {entry['detail']} |")
    lines += [
        "",
        "Raw samples, exact workloads, loaded module hash and environment are in `results.json`.",
        "",
    ]
    (output / "summary.md").write_text("\n".join(lines), encoding="utf-8")


def parser():
    value = argparse.ArgumentParser(description=__doc__)
    value.add_argument(
        "--smoke",
        action="store_true",
        help="reduce paths and portfolio sizes; use only for correctness/smoke checks",
    )
    value.add_argument(
        "--samples",
        type=int,
        help="measured invocations per case (default: 10 full, 1 smoke)",
    )
    value.add_argument(
        "--warmups",
        type=int,
        help="untimed warmup invocations (default: 2 full, 0 smoke)",
    )
    value.add_argument(
        "--group",
        action="append",
        default=[],
        help="C++ target, repeatable; e.g. rate_risk_perf",
    )
    value.add_argument("--filter", default="", help="case-name substring")
    value.add_argument(
        "--list", action="store_true", help="list the selected case names and workloads"
    )
    value.add_argument(
        "--coverage",
        action="store_true",
        help="print all native targets and Python coverage; no DAL import needed",
    )
    value.add_argument(
        "--output-dir", type=Path, default=Path("benchmark-results/python")
    )
    return value


def arguments(argv):
    cli = parser()
    args = cli.parse_args(argv)
    if args.samples is None:
        args.samples = 1 if args.smoke else 10
    if args.warmups is None:
        args.warmups = 0 if args.smoke else 2
    if args.samples <= 0 or args.warmups < 0:
        cli.error("--samples must be positive and --warmups nonnegative")
    return cli, args


def run_case(case, samples, warmups):
    try:
        result = measure(case, samples=samples, warmups=warmups)
        print(f"{case.name}: {result['min_ns'] / 1e6:.6f} ms (min)", flush=True)
        return result
    except Exception as error:
        print(f"{case.name}: FAILED: {error}", file=sys.stderr, flush=True)
        return dict(
            case.description(),
            status="failed",
            error=f"{type(error).__name__}: {error}",
        )


def run_cases(cases, args):
    import dal

    report = {
        "schema": "dal.python-benchmarks/1",
        "profile": "smoke" if args.smoke else "full",
        "samples": args.samples,
        "warmups": args.warmups,
        "environment": environment(),
        "coverage": CPP_COVERAGE,
        "status": "running",
        "results": [],
    }
    # Invalidate any older success report before running the first workload.
    write_report(args.output_dir, report)
    original_date = dal.EvaluationDate_Get()
    try:
        for case in cases:
            report["results"].append(run_case(case, args.samples, args.warmups))
            write_report(args.output_dir, report)
    finally:
        dal.EvaluationDate_Set(original_date)
    report["status"] = (
        "passed"
        if all(row["status"] == "passed" for row in report["results"])
        else "failed"
    )
    write_report(args.output_dir, report)
    return 0 if report["status"] == "passed" else 1


def main(argv=None):
    cli, args = arguments(argv)
    if args.coverage:
        print(json.dumps(CPP_COVERAGE, indent=2))
        return 0
    # Must precede the first DAL import, where the native pool is initialized.
    os.environ.setdefault("DAL_NUM_THREADS", "4")
    try:
        cases = select_cases(build_cases(args.smoke), args.group, args.filter)
    except ValueError as error:
        cli.error(str(error))
    if args.list:
        for case in cases:
            print(json.dumps(case.description(), sort_keys=True))
        return 0
    return run_cases(cases, args)

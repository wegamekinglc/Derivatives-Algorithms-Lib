#!/usr/bin/env python3
"""Gate the complete Python suite against independent base/head DAL builds."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys


TIMEOUT_SECONDS = 600
ROOT = Path(__file__).resolve().parents[2]
WORKER = Path(__file__).with_name("python_benchmark_worker.py")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate_report(report):
    require(isinstance(report, dict), "benchmark report must be an object")
    require(
        report.get("schema") == "dal.python-benchmarks/1",
        "unknown Python benchmark schema",
    )
    require(
        report.get("status") == "passed" and report.get("profile") == "full",
        "failed, incomplete or smoke benchmark report",
    )
    require(
        report.get("samples") == 1 and report.get("warmups") == 2,
        "expected one measured sample and two warmups per process",
    )
    rows = report.get("results", [])
    require(isinstance(rows, list) and bool(rows), "no Python benchmark cases")
    values, descriptions = {}, {}
    for row in rows:
        require(isinstance(row, dict), "benchmark case must be an object")
        name = row["name"]
        require(
            name not in values and row.get("status") == "passed",
            f"duplicate or failed case: {name}",
        )
        samples = row.get("samples_ns", [])
        require(len(samples) == 1, f"incomplete samples: {name}")
        value = samples[0]
        require(
            type(value) in (int, float) and math.isfinite(value) and value > 0,
            f"invalid duration: {name}",
        )
        require(row.get("min_ns") == value, f"minimum differs from raw sample: {name}")
        require(row["workload"].get("profile") == "full", f"non-full workload: {name}")
        values[name] = value
        descriptions[name] = {
            key: row[key] for key in ("name", "cpp_target", "workload")
        }
    environment = report["environment"]
    require(isinstance(environment, dict), "benchmark environment must be an object")
    require(
        bool(environment.get("suite_sha256"))
        and bool(environment.get("native_sha256")),
        "missing source/module hashes",
    )
    require(
        bool(environment.get("python"))
        and bool(environment.get("thread_environment", {}).get("DAL_NUM_THREADS")),
        "missing Python or native thread configuration",
    )
    return values, descriptions


def removed_cases(base_inventory, head_inventory):
    base = {case["name"] for case in base_inventory}
    head = {case["name"] for case in head_inventory}
    require(bool(base) and bool(head), "empty benchmark inventory")
    require(
        len(base) == len(base_inventory) and len(head) == len(head_inventory),
        "duplicate inventory cases",
    )
    return sorted(base - head)


def run_worker(suite, package, output, inventory=False):
    output.mkdir(parents=True, exist_ok=True)
    path = output / ("inventory.json" if inventory else "results.json")
    path.unlink(missing_ok=True)
    command = [
        sys.executable,
        "-s",
        str(WORKER),
        "--suite",
        str(suite.resolve()),
        "--package",
        str(package.resolve()),
        "--output",
        str(output.resolve()),
    ]
    if inventory:
        command.append("--inventory")
    environment = os.environ.copy()
    environment["PYTHONPATH"] = ""
    environment["PYTHONNOUSERSITE"] = "1"
    environment.setdefault("DAL_NUM_THREADS", "4")
    try:
        completed = subprocess.run(
            command,
            cwd=output,
            env=environment,
            shell=False,
            capture_output=True,
            text=True,
            check=False,
            timeout=TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired as error:
        (output / "worker.log").write_text(
            f"worker timed out after {TIMEOUT_SECONDS}s\n{error.stdout or ''}\n{error.stderr or ''}\n",
            encoding="utf-8",
        )
        raise RuntimeError(
            f"Python benchmark worker timed out; see {output}"
        ) from error
    (output / "worker.log").write_text(
        completed.stdout + completed.stderr, encoding="utf-8"
    )
    if completed.returncode:
        raise RuntimeError(
            f"Python benchmark worker exited {completed.returncode}; see {output}"
        )
    report = json.loads(path.read_text(encoding="utf-8"))
    if not inventory:
        validate_report(report)
        native = Path(report["environment"]["native_module"]).resolve()
        require(
            native.is_relative_to(package.resolve() / "dal"),
            "reported native module is outside selected build",
        )
        require(
            hashlib.sha256(native.read_bytes()).hexdigest()
            == report["environment"]["native_sha256"],
            "reported native module hash changed",
        )
    return report


def collect(suite, packages, sample_count, output):
    collected = {"base": [], "head": []}
    for sample in range(sample_count):
        order = ("base", "head") if sample % 2 == 0 else ("head", "base")
        for side in order:
            print(
                f"Python gate: process {sample + 1}/{sample_count} {side}", flush=True
            )
            result = run_worker(
                suite, packages[side], output / "raw" / f"{sample + 1:02d}-{side}"
            )
            collected[side].append(result)
    return collected


def comparable_samples(base, head):
    baseline_environment = base[0]["environment"]
    _, expected = validate_report(base[0])
    values = {"base": {}, "head": {}}
    for side, reports in (("base", base), ("head", head)):
        first = reports[0]["environment"]
        for report in reports:
            samples, descriptions = validate_report(report)
            require(
                descriptions == expected,
                "case inventory or workload differs between processes/sides",
            )
            environment = report["environment"]
            require(
                environment["native_sha256"] == first["native_sha256"],
                f"{side} native binary changed during sampling",
            )
            for key in ("suite_sha256", "python", "thread_environment"):
                require(
                    environment.get(key) == baseline_environment.get(key),
                    f"incomparable {key} between processes/sides",
                )
            for name, duration in samples.items():
                values[side].setdefault(name, []).append(duration)
    return values


def evaluate(base, head, samples, rounds, threshold):
    require(len(base) == len(head) == samples * rounds, "incomplete process samples")
    require(samples > 0 and rounds > 0, "empty sampling schedule")
    values = comparable_samples(base, head)
    rows, failures = [], []
    for name in sorted(values["base"]):
        base_values, head_values = values["base"][name], values["head"][name]
        round_minima = [
            (
                min(base_values[i * samples : (i + 1) * samples]),
                min(head_values[i * samples : (i + 1) * samples]),
            )
            for i in range(rounds)
        ]
        deltas = [100 * (h - b) / b for b, h in round_minima]
        # Cross multiplication preserves the strict boundary at exactly +4%.
        passed = not all(100 * h > (100 + threshold) * b for b, h in round_minima)
        rows.append(
            {
                "case": name,
                "base_ns": min(base_values),
                "head_ns": min(head_values),
                "round_delta_percent": deltas,
                "round_minima_ns": round_minima,
                "passed": passed,
            }
        )
        if not passed:
            failures.append(
                f"{name}: every confirmation round exceeds +{threshold:g}% ({', '.join(f'{x:+.2f}%' for x in deltas)})"
            )
    return rows, failures


def build_configuration(build, source):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.startswith(("#", "//")):
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    require(cache.get("CMAKE_BUILD_TYPE") == "Release", f"not a Release build: {build}")
    require(
        cache.get("DAL_BUILD_PYTHON", "").upper() == "ON", f"Python disabled: {build}"
    )
    require(
        Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == source.resolve(),
        f"build/source mismatch: {build}",
    )
    return {
        key: value
        for key, value in cache.items()
        if key.startswith(("DAL_USE_", "CMAKE_CXX_FLAGS", "DAL_ENABLE_NATIVE_ARCH"))
        or key
        in (
            "CMAKE_BUILD_TYPE",
            "CMAKE_CXX_COMPILER",
            "BUILD_SHARED_LIBS",
            "Python3_EXECUTABLE",
        )
    }


def source_sha(source):
    return subprocess.run(
        ["git", "-C", str(source), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        check=True,
        timeout=10,
    ).stdout.strip()


def write_report(output, result, summary_file=None):
    output.mkdir(parents=True, exist_ok=True)
    (output / "results.json").write_text(
        json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )
    lines = [
        "## Python interface performance regression gate",
        "",
        f"Status: **{result['status']}**.",
        "",
        f"{result['rounds']} rounds x {result['samples']} interleaved processes per side; best-of-N, strict +{result['threshold_percent']:g}% in every round.",
        "",
        "Both native builds run the same head benchmark suite at full scale. Every current case is compared, including on first introduction.",
        "",
        "| Case | Base min (ms) | Head min (ms) | Round changes | Result |",
        "|---|---:|---:|---|---|",
    ]
    for row in result.get("comparisons", []):
        changes = ", ".join(f"{value:+.2f}%" for value in row["round_delta_percent"])
        lines.append(
            f"| {row['case']} | {row['base_ns'] / 1e6:.6f} | {row['head_ns'] / 1e6:.6f} | {changes} | {'pass' if row['passed'] else 'FAIL'} |"
        )
    lines += [
        "",
        *[f"- {failure}" for failure in result.get("failures", [])],
        "",
        "Raw per-process reports and logs are in `raw/`; source/build identities and sampling data are in `results.json`.",
        "",
    ]
    rendered = "\n".join(lines)
    (output / "summary.md").write_text(rendered, encoding="utf-8")
    if summary_file:
        with summary_file.open("a", encoding="utf-8") as stream:
            stream.write(rendered)


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-root", type=Path, required=True)
    parser.add_argument("--head-root", type=Path, required=True)
    parser.add_argument("--base-source", type=Path, required=True)
    parser.add_argument("--head-source", type=Path, default=ROOT)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--summary-file", type=Path)
    parser.add_argument("--samples", type=int, default=10)
    parser.add_argument("--confirmation-rounds", type=int, default=2)
    parser.add_argument("--threshold-percent", type=float, default=4)
    args = parser.parse_args(argv)
    if args.samples < 10 or args.confirmation_rounds < 2:
        parser.error("at least 10 samples and 2 confirmation rounds are required")
    if not math.isfinite(args.threshold_percent) or args.threshold_percent < 0:
        parser.error("threshold must be finite and nonnegative")
    return args


def run_gate(args, result):
    builds = {"base": args.base_root.resolve(), "head": args.head_root.resolve()}
    sources = {"base": args.base_source.resolve(), "head": args.head_source.resolve()}
    require(
        builds["base"] != builds["head"],
        "base and head must use independent build roots",
    )
    configurations = {
        side: build_configuration(builds[side], sources[side]) for side in builds
    }
    require(
        configurations["base"] == configurations["head"],
        "base/head build configurations differ",
    )
    result["builds"] = {
        side: {
            "path": str(builds[side]),
            "source": str(sources[side]),
            "sha": source_sha(sources[side]),
            "configuration": configurations[side],
        }
        for side in builds
    }
    packages = {side: build / "dal-python" for side, build in builds.items()}
    suite = sources["head"] / "dal-python/benchmarks"
    inventory = run_worker(
        suite, packages["head"], args.output_dir / "head-inventory", inventory=True
    )
    removed_cases(
        inventory, inventory
    )  # Validate nonempty, unique head inventory on first introduction too.
    base_suite = sources["base"] / "dal-python/benchmarks"
    if base_suite.exists():
        base_inventory = run_worker(
            base_suite,
            packages["base"],
            args.output_dir / "base-inventory",
            inventory=True,
        )
        removed = removed_cases(base_inventory, inventory)
        require(not removed, f"removed baseline cases: {removed}")
    result["baseline_has_suite"] = base_suite.exists()
    collected = collect(
        suite, packages, args.samples * args.confirmation_rounds, args.output_dir
    )
    result["comparisons"], result["failures"] = evaluate(
        collected["base"],
        collected["head"],
        args.samples,
        args.confirmation_rounds,
        args.threshold_percent,
    )
    _, measured_descriptions = validate_report(collected["head"][0])
    require(
        measured_descriptions == {row["name"]: row for row in inventory},
        "sampled cases differ from head inventory",
    )
    result["environments"] = {
        side: reports[0]["environment"] for side, reports in collected.items()
    }
    result["raw_samples_ns"] = comparable_samples(collected["base"], collected["head"])
    result["status"] = "failed" if result["failures"] else "passed"


def main(argv=None):
    args = arguments(argv)
    args.output_dir = args.output_dir.resolve()
    result = {
        "schema": "dal.python-performance-gate/1",
        "status": "running",
        "samples": args.samples,
        "rounds": args.confirmation_rounds,
        "threshold_percent": args.threshold_percent,
        "comparisons": [],
        "failures": [],
    }
    write_report(args.output_dir, result)
    try:
        run_gate(args, result)
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        RuntimeError,
        subprocess.SubprocessError,
    ) as error:
        result["status"] = "failed"
        result["failures"].append(f"{type(error).__name__}: {error}")
    write_report(args.output_dir, result, args.summary_file)
    print(
        f"Python performance gate: {result['status']}; {len(result['comparisons'])} cases; {args.output_dir}"
    )
    for failure in result["failures"]:
        print(failure, file=sys.stderr)
    return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())

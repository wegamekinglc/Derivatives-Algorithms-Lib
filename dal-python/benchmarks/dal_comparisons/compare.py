"""Interleaved comparisons: gate parity/completeness and report timing ratios."""

import argparse
import json
import os
from pathlib import Path
import subprocess  # nosec B404
import sys

from dal_benchmarks.harness import require
from .evidence import ROOT, SCHEMA, check_report, source_hashes, timings, write_json
from .scenarios import CONVENTIONS, cases, method
from .suite import BACKENDS


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dal-package", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--summary-file", type=Path)
    parser.add_argument("--samples", type=int, default=10)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--worker", choices=BACKENDS, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    require(args.timeout > 0, "timeout must be positive")
    if args.smoke:
        args.samples, args.rounds = 1, 1
    else:
        require(
            args.samples >= 10 and args.rounds >= 2,
            "full comparison needs >=10 samples and >=2 rounds; "
            "use --smoke for a quick check",
        )
    args.dal_package = args.dal_package.resolve()
    args.output_dir = args.output_dir.resolve()
    return args


def worker_environment():
    env = dict(os.environ)
    env.pop("PYTHONPATH", None)
    env.update(
        PYTHONNOUSERSITE="1",
        DAL_NUM_THREADS="4",
        OMP_NUM_THREADS="1",
        OPENBLAS_NUM_THREADS="1",
        MKL_NUM_THREADS="1",
    )
    return env


def invoke(args, backend, directory, hashes):
    require(backend in BACKENDS, f"unknown comparison backend: {backend}")
    directory.mkdir(parents=True, exist_ok=False)
    command = [
        sys.executable,
        "-s",
        str(ROOT / "run_comparisons.py"),
        "--worker",
        backend,
        "--dal-package",
        str(args.dal_package),
        "--output-dir",
        str(directory),
    ]
    if args.smoke:
        command.append("--smoke")
    # Fixed interpreter and local entry point; backend is from BACKENDS, paths
    # are literal arguments and never interpreted by a shell.
    with (directory / "worker.log").open("w", encoding="utf-8") as log:
        subprocess.run(  # nosec B603  # nosemgrep
            command,
            shell=False,
            check=True,
            cwd=ROOT,
            env=worker_environment(),
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=args.timeout,
        )
    report = json.loads((directory / "worker.json").read_text(encoding="utf-8"))
    check_report(report, backend, args.smoke, hashes)
    return report


def stable_provenance(report):
    env = report["environment"]
    return {
        "versions": report["versions"],
        "backend_files": report["backend_files"],
        "native_sha256": env["native_sha256"],
        "cmake": env["cmake"],
        "python": env["python"],
        "thread_environment": env["thread_environment"],
    }


def collect_round(args, round_index, hashes, identities):
    reports = {backend: [] for backend in BACKENDS}
    order = []
    for sample in range(args.samples):
        # Rotate all three backends and reverse alternate rounds.
        offset = sample % len(BACKENDS)
        backends = BACKENDS[offset:] + BACKENDS[:offset]
        if round_index % 2:
            backends = backends[::-1]
        order.append(list(backends))
        for backend in backends:
            directory = (
                args.output_dir
                / f"round-{round_index + 1}"
                / f"sample-{sample + 1}-{backend}"
            )
            report = invoke(args, backend, directory, hashes)
            identity = stable_provenance(report)
            require(
                identities.setdefault(backend, identity) == identity,
                "backend provenance changed during sampling",
            )
            reports[backend].append(report)
    return {"order": order, "cases": aggregate(reports, args.smoke)}


def aggregate(reports, smoke):
    rows = []
    for case in cases(smoke):
        measurements = {
            backend: dict(
                timings(reports[backend], case["name"]), method=method(backend, case)
            )
            for backend in BACKENDS
        }
        dal_time = measurements["dal"]["min_ns"]
        ratios = {
            backend: measurements[backend]["min_ns"] / dal_time
            for backend in BACKENDS[1:]
        }
        rows.append(
            {"workload": case, "backends": measurements, "third_party_over_dal": ratios}
        )
    return rows


def summary(report):
    lines = [
        "# Python third-party comparison",
        "",
        f"Status: {report['status']}; smoke: {report['smoke']}.",
        "",
        "Correctness and complete execution are required. Timing ratios are "
        "informational; DAL base/head regression remains a separate 4% gate.",
        "",
        "Node DV01: DAL reverse AAD; rateslib forward AD (Dual); "
        "QuantLib central finite difference (0.01bp step). "
        "Each returns all 21 non-anchor buckets per trade, in USD/bp. "
        "Curve construction is excluded; differentiation, conversion and "
        "QuantLib relinking/repricing are timed.",
        "",
    ]
    if report["status"] != "passed":
        lines += [f"Error: {report['error']}", "See the retained worker logs.", ""]
        return "\n".join(lines)
    lines += [
        "Ratio = third-party minimum / DAL minimum; >1 means DAL took less time.",
        "",
        "| Round | Case | DAL min ms | QuantLib min ms | rateslib min ms "
        "| QL / DAL | RL / DAL |",
        "| ----- | ---- | ---------: | --------------: | --------------: "
        "| -------: | -------: |",
    ]
    for index, result in enumerate(report["rounds"], 1):
        for row in result["cases"]:
            values = row["backends"]
            times = " | ".join(
                f"{values[backend]['min_ns'] / 1e6:.4f}" for backend in BACKENDS
            )
            ratios = row["third_party_over_dal"]
            lines.append(
                f"| {index} | {row['workload']['name']} | {times} "
                f"| {ratios['quantlib']:.3f} | {ratios['rateslib']:.3f} |"
            )
    return "\n".join(lines) + "\n"


def run(args):
    args.output_dir.mkdir(parents=True, exist_ok=False)
    report = {
        "schema": SCHEMA,
        "status": "failed",
        "smoke": args.smoke,
        "samples_per_round": args.samples,
        "warmups": 2,
        "conventions": CONVENTIONS,
        "source_hashes": source_hashes(),
        "identities": {},
        "rounds": [],
    }
    try:
        for index in range(args.rounds):
            report["rounds"].append(
                collect_round(
                    args, index, report["source_hashes"], report["identities"]
                )
            )
        report["status"] = "passed"
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        subprocess.SubprocessError,
    ) as exc:
        report["error"] = f"{type(exc).__name__}: {exc}"
    write_json(args.output_dir / "results.json", report)
    markdown = summary(report)
    (args.output_dir / "summary.md").write_text(markdown, encoding="utf-8")
    if args.summary_file:
        with args.summary_file.open("a", encoding="utf-8") as stream:
            stream.write(markdown)
    print(markdown)
    return 0 if report["status"] == "passed" else 1


def main(argv=None):
    args = arguments(argv)
    if args.worker:
        from .worker import run as run_worker

        return run_worker(args)
    return run(args)

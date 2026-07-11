#!/usr/bin/env python3
"""Run DAL benchmarks against base and head in an interleaved regression gate."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


# Per-process wall-clock cap. The regression binaries finish in seconds; this
# only exists to fail loudly on a deadlock (e.g. a thread-pool regression)
# instead of stalling the job to the GitHub Actions 6h cap.
BENCHMARK_TIMEOUT_SECONDS = 600

BENCHMARKS = (
    "tape_perf",
    "jacobian_perf",
    "pde_perf",
    "rng_perf",
    "interp_perf",
    "krylov_perf",
    "banded_perf",
    "cholesky_perf",
)
FAST_SOBOL_CASE = "Sobol FillNormal fast (100K x 10D)"
OLD_PRECISE_SOBOL_CASE = "Sobol FillNormal precise (100K x 10D)"
PRECISE_SOBOL_CASE = "Sobol FillNormal precise opt-in (100K x 10D)"
UNIT_TO_NS = {"ns": 1.0, "us": 1_000.0, "ms": 1_000_000.0, "s": 1_000_000_000.0}
NUMBER = r"[0-9]+(?:\.[0-9]+)?"
ROW = re.compile(
    rf"^(?P<name>\S(?:.*?\S)?)\s+"
    rf"(?P<median>{NUMBER})\s+(?P<median_unit>ns|us|ms|s)\s+"
    rf"(?P<minimum>{NUMBER})\s+(?P<minimum_unit>ns|us|ms|s)\s+"
    rf"(?P<maximum>{NUMBER})\s+(?P<maximum_unit>ns|us|ms|s)\s+"
    r"(?P<reps>[0-9]+)\s*$"
)


def parse_benchmark_output(output: str) -> dict[str, float]:
    """Return each case's reported minimum duration in nanoseconds."""
    results = {}
    for line in output.splitlines():
        match = ROW.match(line)
        if match:
            results[match.group("name")] = float(match.group("minimum")) * UNIT_TO_NS[match.group("minimum_unit")]
    if not results:
        raise ValueError("benchmark output contained no result rows")
    return results


def benchmark_binary(build_root: Path, benchmark: str) -> Path:
    if benchmark not in BENCHMARKS:
        raise ValueError(f"unsupported benchmark: {benchmark}")

    resolved_root = build_root.resolve()
    binary = (resolved_root / "dal-cpp" / "benchmarks" / benchmark / benchmark).resolve()
    if not binary.is_relative_to(resolved_root):
        raise ValueError(f"benchmark binary escapes build root: {binary}")
    if not binary.is_file():
        raise FileNotFoundError(f"benchmark binary not found: {binary}")
    if not os.access(binary, os.X_OK):
        raise PermissionError(f"benchmark binary is not executable: {binary}")
    return binary


def run_benchmark(build_root: Path, benchmark: str, output_file: Path) -> dict[str, float]:
    binary = benchmark_binary(build_root, benchmark)
    environment = os.environ.copy()
    environment.setdefault("DAL_NUM_THREADS", "4")
    # Running the validated, locally built executable is this tool's purpose; no shell parses the path.
    try:
        completed = subprocess.run(  # nosemgrep
            [str(binary)],  # nosemgrep
            check=False,
            capture_output=True,
            text=True,
            env=environment,
            shell=False,
            timeout=BENCHMARK_TIMEOUT_SECONDS,
        )
    except subprocess.TimeoutExpired:
        output_file.write_text(
            f"{binary.name} timed out after {BENCHMARK_TIMEOUT_SECONDS}s\n", encoding="utf-8"
        )
        raise RuntimeError(
            f"{binary.name} exceeded the {BENCHMARK_TIMEOUT_SECONDS}s timeout; see {output_file}"
        )
    output_file.write_text(completed.stdout + completed.stderr, encoding="utf-8")
    if completed.returncode:
        raise RuntimeError(f"{binary.name} exited with {completed.returncode}; see {output_file}")
    return parse_benchmark_output(completed.stdout)


def collect_samples(
    base_root: Path,
    head_root: Path,
    benchmarks: list[str],
    sample_count: int,
    output_dir: Path,
) -> dict[str, dict[str, dict[str, list[float]]]]:
    """Alternate base/head first position on every outer sample."""
    collected = {}
    for benchmark in benchmarks:
        benchmark_dir = output_dir / benchmark
        benchmark_dir.mkdir(parents=True, exist_ok=True)
        sides = {"base": {}, "head": {}}
        roots = {"base": base_root, "head": head_root}
        for sample in range(sample_count):
            order = ("base", "head") if sample % 2 == 0 else ("head", "base")
            for side in order:
                output_file = benchmark_dir / f"{sample + 1:02d}-{side}.txt"
                values = run_benchmark(roots[side], benchmark, output_file)
                for case, value in values.items():
                    sides[side].setdefault(case, []).append(value)
        validate_sample_counts(benchmark, sides, sample_count)
        collected[benchmark] = sides
    return collected


def validate_sample_counts(
    benchmark: str,
    sides: dict[str, dict[str, list[float]]],
    expected_count: int,
) -> None:
    for side, cases in sides.items():
        incomplete = {case: len(values) for case, values in cases.items() if len(values) != expected_count}
        if incomplete:
            raise RuntimeError(f"{benchmark} / {side}: incomplete samples {incomplete}")


def min_samples(samples: dict[str, list[float]]) -> dict[str, float]:
    # Gate on best-of-N (min): benchmark timings are right-skewed, so the minimum
    # is the sample least contaminated by scheduler/cache/page-fault transients.
    return {case: min(values) for case, values in samples.items()}


def permitted_case_migration(base_cases: set[str], head_cases: set[str]) -> bool:
    return base_cases == {OLD_PRECISE_SOBOL_CASE} and head_cases == {PRECISE_SOBOL_CASE}


def semantic_migration_row(base: dict[str, float], head: dict[str, float]) -> dict[str, object]:
    base_value = base[OLD_PRECISE_SOBOL_CASE]
    head_value = head[PRECISE_SOBOL_CASE]
    return {
        "case": "Sobol precise policy migration (historical Acklam-only -> exact-CDF opt-in)",
        "base_ns": base_value,
        "head_ns": head_value,
        "delta_percent": 100.0 * (head_value / base_value - 1.0),
        "passed": True,
        "gated": False,
    }


def round_deltas(
    base: list[float],
    head: list[float],
    round_size: int,
    round_count: int,
) -> list[float]:
    deltas = []
    for round_index in range(round_count):
        first = round_index * round_size
        last = first + round_size
        base_min = min(base[first:last])
        head_min = min(head[first:last])
        deltas.append(100.0 * (head_min / base_min - 1.0))
    return deltas


def comparison_row(
    case: str,
    base: list[float],
    head: list[float],
    threshold_percent: float,
    round_size: int,
    round_count: int,
) -> dict[str, object]:
    base_value = min(base)
    head_value = min(head)
    deltas = round_deltas(base, head, round_size, round_count)
    return {
        "case": case,
        "base_ns": base_value,
        "head_ns": head_value,
        "delta_percent": 100.0 * (head_value / base_value - 1.0),
        "round_delta_percent": deltas,
        "passed": not all(delta > threshold_percent for delta in deltas),
        "gated": True,
    }


def benchmark_case_differences(
    benchmark: str,
    base: dict[str, float],
    head: dict[str, float],
) -> tuple[bool, list[str]]:
    base_only = set(base) - set(head)
    head_only = set(head) - set(base)
    migration_permitted = benchmark == "rng_perf" and permitted_case_migration(base_only, head_only)
    if (not base_only and not head_only) or migration_permitted:
        return migration_permitted, []
    return migration_permitted, [
        f"{benchmark}: benchmark cases differ "
        f"(base-only={sorted(base_only)}, head-only={sorted(head_only)})"
    ]


def compare_benchmark(
    benchmark: str,
    samples: dict[str, dict[str, list[float]]],
    threshold_percent: float,
    precise_slowdown_limit: float,
    round_size: int,
    round_count: int,
) -> tuple[list[dict[str, object]], list[str]]:
    base = min_samples(samples["base"])
    head = min_samples(samples["head"])
    migration_permitted, failures = benchmark_case_differences(benchmark, base, head)
    rows = []
    if migration_permitted:
        rows.append(semantic_migration_row(base, head))
    for case in sorted(set(base) & set(head)):
        row = comparison_row(
            case,
            samples["base"][case],
            samples["head"][case],
            threshold_percent,
            round_size,
            round_count,
        )
        rows.append(row)
        if not row["passed"]:
            formatted_deltas = ", ".join(f"{delta:+.2f}%" for delta in row["round_delta_percent"])
            failures.append(
                f"{benchmark} / {case}: every confirmation round exceeds "
                f"+{threshold_percent:.2f}% ({formatted_deltas})"
            )

    if benchmark == "rng_perf":
        failures.extend(check_precise_sobol_ratio(head, precise_slowdown_limit))
    return rows, failures


def check_precise_sobol_ratio(head: dict[str, float], limit: float) -> list[str]:
    ratio = precise_sobol_ratio(head)
    if ratio is None:
        return ["rng_perf: fast or precise opt-in Sobol case is missing"]
    if ratio <= limit:
        return []
    return [f"rng_perf / precise opt-in: {ratio:.2f}x fast exceeds {limit:.2f}x"]


def precise_sobol_ratio(head: dict[str, float]) -> float | None:
    if FAST_SOBOL_CASE not in head or PRECISE_SOBOL_CASE not in head:
        return None
    return head[PRECISE_SOBOL_CASE] / head[FAST_SOBOL_CASE]


def evaluate(
    collected: dict[str, dict[str, dict[str, list[float]]]],
    threshold_percent: float,
    precise_slowdown_limit: float,
    round_size: int,
    round_count: int,
) -> tuple[dict[str, list[dict[str, object]]], list[str]]:
    comparisons = {}
    failures = []
    for benchmark, samples in collected.items():
        rows, benchmark_failures = compare_benchmark(
            benchmark,
            samples,
            threshold_percent,
            precise_slowdown_limit,
            round_size,
            round_count,
        )
        comparisons[benchmark] = rows
        failures.extend(benchmark_failures)
    return comparisons, failures


def result_label(row: dict[str, object]) -> str:
    if not row.get("gated", True):
        return "info"
    return "pass" if row["passed"] else "fail"


def markdown_comparison_rows(comparisons: dict[str, list[dict[str, object]]]) -> list[str]:
    lines = []
    for benchmark, rows in comparisons.items():
        for row in rows:
            round_changes = ", ".join(f"{delta:+.2f}%" for delta in row.get("round_delta_percent", [])) or "—"
            lines.append(
                f"| {benchmark} | {row['case']} | {row['base_ns'] / 1_000_000.0:.6f} ms | "
                f"{row['head_ns'] / 1_000_000.0:.6f} ms | {row['delta_percent']:+.2f}% | "
                f"{round_changes} | {result_label(row)} |"
            )
    return lines


def has_semantic_migration(comparisons: dict[str, list[dict[str, object]]]) -> bool:
    return any(not row.get("gated", True) for rows in comparisons.values() for row in rows)


def markdown_report(
    comparisons: dict[str, list[dict[str, object]]],
    failures: list[str],
    sample_count: int,
    round_count: int,
    threshold_percent: float,
    precise_ratio: float | None,
    precise_slowdown_limit: float,
) -> str:
    lines = [
        "## Paired benchmark regression gate",
        "",
        f"{round_count} independent rounds of {sample_count} interleaved process-level samples; "
        f"failure requires every round to exceed +{threshold_percent:.2f}%.",
        "",
        "| Benchmark | Case | Base min | Head min | Change | Round changes | Result |",
        "|---|---|---:|---:|---:|---:|:---:|",
    ]
    lines.extend(markdown_comparison_rows(comparisons))
    if precise_ratio is not None:
        lines.extend(
            [
                "",
                f"Head Sobol precise opt-in / fast ratio: {precise_ratio:.2f}x "
                f"(limit {precise_slowdown_limit:.2f}x).",
            ]
        )
    if has_semantic_migration(comparisons):
        lines.extend(
            [
                "",
                "The informational Sobol migration row compares the historical "
                "`precise=true, polish=false` Acklam-only behavior with the new exact-CDF opt-in. "
                "Comparable cases use the 4% gate; precise opt-in uses the relative ceiling above.",
            ]
        )
    if failures:
        lines.extend(["", "### Failures", ""] + [f"- {failure}" for failure in failures])
    else:
        lines.extend(["", "All performance acceptance checks passed."])
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-root", type=Path, required=True)
    parser.add_argument("--head-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--summary-file", type=Path)
    parser.add_argument("--benchmarks", nargs="+", choices=BENCHMARKS, default=list(BENCHMARKS))
    parser.add_argument("--samples", type=int, default=10)
    parser.add_argument("--confirmation-rounds", type=int, default=2)
    parser.add_argument("--threshold-percent", type=float, default=4.0)
    parser.add_argument("--precise-slowdown-limit", type=float, default=10.0)
    args = parser.parse_args()
    if args.samples < 10:
        parser.error("--samples must be at least 10")
    if args.confirmation_rounds < 2:
        parser.error("--confirmation-rounds must be at least 2")
    return args


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    total_samples = args.samples * args.confirmation_rounds
    collected = collect_samples(args.base_root, args.head_root, args.benchmarks, total_samples, args.output_dir)
    comparisons, failures = evaluate(
        collected,
        args.threshold_percent,
        args.precise_slowdown_limit,
        args.samples,
        args.confirmation_rounds,
    )
    rng_head = min_samples(collected["rng_perf"]["head"]) if "rng_perf" in collected else {}
    precise_ratio = precise_sobol_ratio(rng_head)
    report = markdown_report(
        comparisons,
        failures,
        args.samples,
        args.confirmation_rounds,
        args.threshold_percent,
        precise_ratio,
        args.precise_slowdown_limit,
    )
    (args.output_dir / "summary.md").write_text(report, encoding="utf-8")
    (args.output_dir / "results.json").write_text(
        json.dumps({"samples": collected, "comparisons": comparisons, "failures": failures}, indent=2) + "\n",
        encoding="utf-8",
    )
    if args.summary_file:
        with args.summary_file.open("a", encoding="utf-8") as summary:
            summary.write(report)
    print(report, end="")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

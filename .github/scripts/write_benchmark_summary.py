#!/usr/bin/env python3
"""Aggregate raw DAL benchmark outputs into one step-summary table.

The CI benchmark jobs run each benchmark binary on its own and capture stdout/stderr
into <results-dir>/<bench>.txt, plus the process exit code in <bench>.status. This
script parses every captured file and appends a single markdown table (one row per
benchmark case) to the GitHub step summary, replacing the old per-benchmark sections
that each repeated their own header.

Two row formats are recognised:

- Dal::Bench table rows: "<case> <median> <unit> <min> <unit> <max> <unit> <reps>"
- script_perf-style per-iteration rows: "<case> <value> <unit>/iter"

Anything else (init banners, observation counters, table headers) stays available in
the CI log and the uploaded artifact but is not a table row.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re


NUMBER = r"[0-9]+(?:\.[0-9]+)?"
UNIT = r"(?:ns|us|ms|s)"
TIMED_ROW = re.compile(
    rf"^(?P<case>\S(?:.*?\S)?)\s+"
    rf"(?P<median>{NUMBER})\s+(?P<median_unit>{UNIT})\s+"
    rf"(?P<minimum>{NUMBER})\s+(?P<minimum_unit>{UNIT})\s+"
    rf"(?P<maximum>{NUMBER})\s+(?P<maximum_unit>{UNIT})\s+"
    rf"(?P<reps>[0-9]+)\s*$"
)
PER_ITER_ROW = re.compile(
    rf"^(?P<case>\S(?:.*?\S)?)\s+(?P<value>{NUMBER})\s+(?P<unit>{UNIT})/iter\s*$"
)

EMPTY_CELL = "—"


def parse_rows(text: str) -> list[dict[str, str]]:
    """Parse one benchmark's raw output into table rows, preserving print order."""
    rows = []
    for line in text.splitlines():
        match = TIMED_ROW.match(line)
        if match:
            rows.append(
                {
                    "case": match.group("case"),
                    "median": f"{match.group('median')} {match.group('median_unit')}",
                    "minimum": f"{match.group('minimum')} {match.group('minimum_unit')}",
                    "maximum": f"{match.group('maximum')} {match.group('maximum_unit')}",
                    "reps": match.group("reps"),
                }
            )
            continue
        match = PER_ITER_ROW.match(line)
        if match:
            rows.append(
                {
                    "case": match.group("case"),
                    "median": f"{match.group('value')} {match.group('unit')}/iter",
                    "minimum": EMPTY_CELL,
                    "maximum": EMPTY_CELL,
                    "reps": EMPTY_CELL,
                }
            )
    return rows


def escape_cell(text: str) -> str:
    return text.replace("|", "\\|")


def read_status(results_dir: Path, benchmark: str) -> int | None:
    status_file = results_dir / f"{benchmark}.status"
    if not status_file.is_file():
        return None
    try:
        return int(status_file.read_text(encoding="utf-8").strip())
    except (OSError, ValueError):
        return None


def build_report(benchmarks: list[str], results_dir: Path, configuration: str) -> str:
    lines = [
        "## Benchmark Results",
        "",
        f"Configuration: {configuration}",
        f"Discovered {len(benchmarks)} benchmark(s) via CTest label 'benchmark'.",
        "",
        "| Benchmark | Case | Median | Min | Max | Reps |",
        "|---|---|---:|---:|---:|---:|",
    ]
    failures = []
    for benchmark in benchmarks:
        status = read_status(results_dir, benchmark)
        if status:
            failures.append((benchmark, status))
        result_file = results_dir / f"{benchmark}.txt"
        if not result_file.is_file():
            lines.append(f"| {escape_cell(benchmark)} | (no output captured) | {EMPTY_CELL} | {EMPTY_CELL} | {EMPTY_CELL} | {EMPTY_CELL} |")
            continue
        try:
            output = result_file.read_text(encoding="utf-8", errors="replace")
        except OSError:
            output = ""
        rows = parse_rows(output)
        if not rows:
            rows = [
                {
                    "case": "(no parseable result rows)",
                    "median": EMPTY_CELL,
                    "minimum": EMPTY_CELL,
                    "maximum": EMPTY_CELL,
                    "reps": EMPTY_CELL,
                }
            ]
        for row in rows:
            lines.append(
                f"| {escape_cell(benchmark)} | {escape_cell(row['case'])} | {row['median']} | "
                f"{row['minimum']} | {row['maximum']} | {row['reps']} |"
            )
    if failures:
        formatted = ", ".join(f"`{name}` (exit code {status})" for name, status in failures)
        lines.extend(["", f"Benchmarks that did not exit cleanly: {formatted}."])
    else:
        lines.extend(["", "All benchmarks exited cleanly."])
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results-dir", type=Path, required=True)
    parser.add_argument("--summary-file", type=Path)
    parser.add_argument("--configuration", required=True)
    parser.add_argument("--benchmarks", nargs="+", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report = build_report(args.benchmarks, args.results_dir, args.configuration)
    if args.summary_file:
        with args.summary_file.open("a", encoding="utf-8") as summary:
            summary.write(report)
    print(report, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

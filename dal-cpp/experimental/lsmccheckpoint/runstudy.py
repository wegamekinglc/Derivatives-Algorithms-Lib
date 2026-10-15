#!/usr/bin/env python3
"""Collect interleaved, fresh-process checkpoint measurements on Linux."""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


ROOT = Path(__file__).resolve().parents[3]
CASES = (
    ("hard_blocks", "hard", 65, 8, 8193, 257, False),
    ("hard_one_block", "hard", 65, 65, 8193, 257, False),
    ("growing_history", "hard", 65, 8, 2049, 73, True),
    ("frozen", "frozen", 17, 4, 2049, 73, False),
    ("retrained", "retrained", 17, 4, 513, 73, False),
)


def capture(command):
    return subprocess.check_output(command, cwd=ROOT, text=True).strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_snapshot():
    rows = capture(["ps", "-eo", "pid,comm,pcpu"]).splitlines()[1:]
    return [row.split() for row in rows if row.split()[1] in
            {"cc1plus", "rustc", "ld", "dal_cpp_tests", "script_mc_perf", "rate_risk_perf"}]


def sample(binary, destination, case, schedule):
    name, mode, events, block, training, validation, growing = case
    command = [str(binary), "--schedule", schedule, "--mode", mode,
               "--events", str(events), "--block", str(block), "--training", str(training),
               "--validation", str(validation), "--pricing", "257",
               "--growing-vector", str(growing).lower()]
    resource = destination.with_suffix(".resource.txt")
    process = subprocess.run(["/usr/bin/time", "-v", "-o", str(resource), *command],
                             cwd=ROOT, env={**os.environ, "DAL_NUM_THREADS": "1"},
                             text=True, capture_output=True, check=False)
    destination.with_suffix(".stdout.txt").write_text(process.stdout)
    destination.with_suffix(".stderr.txt").write_text(process.stderr)
    if process.returncode:
        raise RuntimeError(f"{name}/{schedule} exited {process.returncode}: {process.stderr}")
    result = json.loads(process.stdout.splitlines()[-1])
    result["max_rss_kib"] = int(next(line.rsplit(":", 1)[1] for line in resource.read_text().splitlines()
                                     if "Maximum resident set size" in line))
    result["case"] = name
    result["command"] = command
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=10)
    parser.add_argument("--rounds", type=int, default=2)
    options = parser.parse_args()
    if options.samples < 10 or options.rounds < 2:
        parser.error("retain at least two rounds of ten samples per schedule")
    binary = options.binary.resolve()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    provenance = {
        "head": capture(["git", "rev-parse", "HEAD"]),
        "baseline": "e6f57c7237db603256a8d5f99656fb66a639de9a",
        "dirty": capture(["git", "status", "--short"]),
        "binary_sha256": digest(binary),
        "source_sha256": digest(Path(__file__).with_name("checkpointstudy.cpp")),
        "compiler": capture(["c++", "--version"]).splitlines()[0],
        "cmake": capture(["cmake", "--version"]).splitlines()[0],
        "submodules": capture(["git", "submodule", "status"]),
        "platform": platform.platform(),
        "cpu": next(line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                    if line.startswith("model name")),
        "threads": 1,
        "aad": "native",
        "samples_per_round": options.samples,
        "rounds": options.rounds,
        "before_load": load_snapshot(),
        "cmake_cache": (binary.parent / "CMakeCache.txt").read_text(),
        "compile_commands": (binary.parent / "compile_commands.json").read_text(),
        "source_diff": capture(["git", "diff", "HEAD", "--", "dal-cpp/experimental/lsmccheckpoint"]),
    }
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    results = []
    for round_index in range(options.rounds):
        for index in range(options.samples):
            for case in CASES:
                schedules = ("materialized", "checkpoint") if index % 2 == 0 else ("checkpoint", "materialized")
                pair = []
                for schedule in schedules:
                    destination = output / f"{case[0]}-r{round_index}-s{index}-{schedule}"
                    result = sample(binary, destination, case, schedule)
                    result.update(round=round_index, sample=index)
                    pair.append(result)
                    results.append(result)
                if pair[0]["pv"] != pair[1]["pv"] or pair[0]["risks"] != pair[1]["risks"]:
                    raise RuntimeError(f"price/risk mismatch: {case[0]}")
    if digest(binary) != provenance["binary_sha256"]:
        raise RuntimeError("binary changed during sampling")
    provenance["after_load"] = load_snapshot()
    provenance["timing_verdict"] = "informational; no nine-target regression verdict"
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    fields = ["case", "schedule", "round", "sample", "seconds", "max_rss_kib", "peak_owned_storage_bytes",
              "checkpoint_bytes", "normal_vectors", "normal_coordinates", "model_steps", "bootstrap_model_steps", "script_events", "pv"]
    with (output / "samples.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(results)
    summaries = []
    for case in CASES:
        summary = {"case": case[0]}
        for schedule in ("materialized", "checkpoint"):
            samples = [row for row in results if row["case"] == case[0] and row["schedule"] == schedule]
            summary[schedule] = {
                "min_seconds_by_round": [min(row["seconds"] for row in samples if row["round"] == round_index)
                                         for round_index in range(options.rounds)],
                "min_seconds": min(row["seconds"] for row in samples),
                "rss_kib_range": [min(row["max_rss_kib"] for row in samples), max(row["max_rss_kib"] for row in samples)],
                **{key: samples[0][key] for key in ("peak_owned_storage_bytes", "checkpoint_bytes", "normal_vectors",
                                                   "normal_coordinates", "model_steps", "bootstrap_model_steps", "script_events", "pv", "risks")},
            }
        summaries.append(summary)
    (output / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
    print(json.dumps(summaries, indent=2))


if __name__ == "__main__":
    main()

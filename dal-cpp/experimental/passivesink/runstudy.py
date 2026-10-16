#!/usr/bin/env python3
"""Run paired sink-overlay measurements; hold the shared measurement lock externally."""

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess  # nosec B404: explicitly selected local build-tree executables, without a shell
import sys
import time


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fingerprint(rows):
    def exact(value):
        if isinstance(value, float):
            require(math.isfinite(value), "Nonfinite result")
            return struct.pack("!d", value).hex()
        if isinstance(value, list):
            return [exact(item) for item in value]
        if isinstance(value, dict):
            return {key: exact(item) for key, item in value.items() if key != "record"}
        return value
    return exact(rows)


def selected_jobs():
    cases = [("short", 65536, "compiled", 1), ("long", 8192, "compiled", 1),
             ("local-vol", 2048, "compiled", 1), ("gsr-swap-3f", 8192, "compiled", 1),
             ("slv-swap-3f", 4096, "compiled", 1), ("long", 8192, "tree", 1), ("long", 8192, "compiled", 4)]
    jobs = [(f"mc-{scenario}-{engine}-t{threads}", "production", scenario, paths, engine, threads)
            for scenario, paths, engine, threads in cases]
    jobs += [(f"chain-{kind}-w{width}", "chain", kind, width, "", 1)
             for kind, width in [("number", 1), ("double", 1), ("number", 4)]]
    return jobs


def validate_builds(run, binaries, root):
    for side in ["base", "head"]:
        run(f"validate-{side}", [binaries[side]["tests"]])
        run(f"validate-profile-{side}", [sys.executable, root / f"{side}-source/tests/aad-profiling/check_production_profile.py",
                                         binaries[side]["production"], "--profiling", "off"])
        for scenario in ["short", "long", "local-vol", "gsr-swap-3f", "slv-swap-3f"]:
            grid = "8" if scenario == "local-vol" else "0"
            run(f"structure-{scenario}-{side}", [binaries[side]["production"], "--production-profile", scenario,
                                                 "8", "aad", "compiled", "structure", "1", grid, "0", "1"])


def summarize(samples):
    results = []
    for name, data in samples.items():
        require(all(len(round_values) == 10 for side in data.values() for round_values in side), f"Incomplete sample set: {name}")
        require(all(len(side) == 2 for side in data.values()), f"Incomplete rounds: {name}")
        base, head = [min(r) for r in data["base"]], [min(r) for r in data["head"]]
        deltas = [(h / b - 1) * 100 for b, h in zip(base, head)]
        verdict = "regression" if all(d > 4 for d in deltas) else "improvement" if all(d < -4 for d in deltas) else "no-change"
        results.append({"case": name, "base_min_ms": min(base) / 1e6, "sink_min_ms": min(head) / 1e6,
                        "round_1_percent": deltas[0], "round_2_percent": deltas[1], "verdict": verdict})
    return results


def verify_binaries(binaries, identity):
    for side, paths in binaries.items():
        for key, path in paths.items():
            require(digest(path) == identity[side][key]["sha256"], "Binary changed during sampling")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.work_root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    raw = output / "raw"
    raw.mkdir()
    allowed = sorted(os.sched_getaffinity(0))
    require(len(allowed) >= 4, "Four CPUs required for the selected worker boundary")
    binaries = {side: {
        "chain": root / f"{side}-build/passive_sink_study",
        "production": root / f"{side}-build/dal/dal-cpp/benchmarks/script_mc_perf/script_mc_perf",
        "tests": root / f"{side}-build/passive_sink_tests",
    } for side in ["base", "head"]}
    identity = {side: {key: {"path": str(path), "sha256": digest(path)} for key, path in paths.items()}
                for side, paths in binaries.items()}
    commands, samples, fingerprints = [], {}, {}

    def run(label, command, threads=1):
        ordinal = len(commands)
        cpu = allowed[:threads]
        argv = ["taskset", "-c", ",".join(map(str, cpu)), *map(str, command)]
        env = {**os.environ, "DAL_NUM_THREADS": str(threads)}
        start = time.time()
        stdout = raw / f"{ordinal:04d}-{label}.stdout"
        stderr = stdout.with_suffix(".stderr")
        resource = stdout.with_suffix(".resources.json")
        timed = ["/usr/bin/time", "-f", '{"rss_kb":%M,"user_seconds":%U,"system_seconds":%S}', "-o", str(resource), *argv]
        # Executables and workload arguments come from the resolved study/build paths and fixed case list.
        result = subprocess.run(timed, env=env, capture_output=True, text=True, check=False, timeout=240, shell=False)  # nosemgrep # nosec B603
        stdout.write_text(result.stdout)
        stderr.write_text(result.stderr)
        commands.append({"command": argv, "threads": threads, "returncode": result.returncode,
                         "start_unix": start, "elapsed_s": time.time() - start,
                         "stdout": str(stdout.relative_to(output)), "stderr": str(stderr.relative_to(output)),
                         "resources": str(resource.relative_to(output))})
        (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        require(result.returncode == 0, f"Command failed: {label}; see {stderr}")
        return result.stdout

    validate_builds(run, binaries, root)
    jobs = selected_jobs()
    environment = {"executables": identity, "cpus": allowed[:4], "rounds": 2, "process_samples_per_side_per_round": 10,
                   "reduction": "min", "threshold_percent": 4, "source_base": subprocess.check_output(
                       ["git", "-C", str(root / "base-source"), "rev-parse", "HEAD"], text=True).strip(),
                   "overlay_sha256": digest(Path(__file__).with_name("native-sink.patch")),
                   "uname": list(os.uname())}
    (output / "environment.json").write_text(json.dumps(environment, indent=2) + "\n")

    def collect(round_index, sample_index, job, side):
        name, kind, scenario, size, engine, threads = job
        if kind == "production":
            grid = "8" if scenario == "local-vol" else "0"
            command = [binaries[side][kind], "--production-profile", scenario, str(size), "aad", engine, "cold", "1", grid, "0", "1"]
        else:
            command = [binaries[side][kind], scenario, str(size)]
        text = run(f"r{round_index}-s{sample_index}-{name}-{side}", command, threads)
        rows = [json.loads(line, parse_int=float) for line in text.splitlines() if line.startswith("{")]
        if kind == "production":
            request = next(row for row in rows if row["record"] == "request")
            values = [row for row in rows if row["record"] == "result"]
            require(values and request["paths_per_output"] == size and request["threads"] == threads, f"Wrong workload: {name}")
            metrics = {"wall_ns": request["wall_ns"]}
        else:
            require(len(rows) == 1, f"Wrong chain output: {name}")
            row = rows[0]
            metrics = {key: row[key] for key in ["record_ns", "reverse_ns"]}
            values = {key: row[key] for key in ["value", "gradient"]}
            (output / f"{name}-{side}-structure.json").write_text(json.dumps(row, indent=2) + "\n")
        current = fingerprint(values)
        if name in fingerprints:
            require(current == fingerprints[name], f"PV/risk bit mismatch: {name}/{side}")
        else:
            fingerprints[name] = current
        for metric, value in metrics.items():
            require(value > 0, f"Invalid elapsed value: {name}/{metric}")
            key = f"{name}/{metric}"
            entry = samples.setdefault(key, {"base": [[], []], "head": [[], []]})
            entry[side][round_index].append(value)
        (output / "raw-samples.json").write_text(json.dumps(samples, indent=2) + "\n")
        (output / "fingerprints.json").write_text(json.dumps(fingerprints, indent=2) + "\n")

    for round_index in range(2):
        (output / f"load-round-{round_index}.txt").write_text(subprocess.check_output(
            ["ps", "-eo", "pid,comm,pcpu,pmem,args", "--sort=-pcpu"], text=True))
        for sample_index in range(10):
            for job in jobs:
                order = ["base", "head"] if (round_index + sample_index) % 2 == 0 else ["head", "base"]
                for side in order:
                    collect(round_index, sample_index, job, side)
        print(f"Round {round_index + 1}/2 complete", flush=True)
    results = summarize(samples)
    verify_binaries(binaries, identity)
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    with (output / "results.csv").open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(results[0]))
        writer.writeheader()
        writer.writerows(results)
    print(json.dumps(results, indent=2), flush=True)


if __name__ == "__main__":
    main()

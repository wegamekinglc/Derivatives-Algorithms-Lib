#!/usr/bin/env python3
"""Run the isolated replay experiment; invoke the entire script under the shared flock."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess


def run(command):
    return subprocess.run(command, check=True, text=True, capture_output=True).stdout


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def close(left, right):
    return math.isfinite(left) and math.isfinite(right) and abs(left - right) <= 2e-10 * max(1.0, abs(left), abs(right))


def compare(reference, result):
    if not close(reference["pv"], result["pv"]) or reference["risks"].keys() != result["risks"].keys():
        raise RuntimeError("PV or risk coordinates changed")
    for label, value in reference["risks"].items():
        if not close(value, result["risks"][label]):
            raise RuntimeError(f"risk mismatch: {label}: {value} vs {result['risks'][label]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--paths", type=int, default=32768)
    parser.add_argument("--cpu", type=int, required=True)
    parser.add_argument("--cases", nargs="+", choices=("vanilla", "barrier", "crossing"), default=("vanilla", "barrier", "crossing"))
    args = parser.parse_args()
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parents[3]
    build = binary.parent
    os.sched_setaffinity(0, {args.cpu})
    os.environ["DAL_NUM_THREADS"] = "1"
    modes = ("native", "replay", "record", "caller", "native-aa")
    cases = tuple(args.cases)
    identity = {
        "source_head": run(["git", "-C", str(repo), "rev-parse", "HEAD"]).strip(),
        "diff": run(["git", "-C", str(repo), "diff", "HEAD"]),
        "sources": {str(p.relative_to(repo)): digest(p) for p in Path(__file__).resolve().parent.iterdir() if p.is_file()},
        "production_tree": run(["git", "-C", str(repo), "rev-parse", "HEAD:dal-cpp/dal"]).strip(),
        "binary_sha256": digest(binary),
        "library_sha256": digest(build / "dal/dal-cpp/libdal_cpp.a"),
        "cache_sha256": digest(build / "CMakeCache.txt"),
        "flags": (build / "CMakeFiles/primal_replay_study.dir/flags.make").read_text(),
        "submodules": run(["git", "-C", str(repo), "submodule", "status"]),
        "compiler": run(["c++", "--version"]),
        "cmake": run(["cmake", "--version"]),
        "cpu": run(["lscpu"]),
        "load_before": run(["ps", "-eo", "pid,pcpu,comm,args", "--sort=-pcpu"]),
        "platform": platform.platform(),
        "affinity": sorted(os.sched_getaffinity(0)),
        "threads": 1,
        "paths": args.paths,
        "cases": cases,
        "samples_per_round": 10,
        "rounds": 2,
        "reduction": "min",
        "threshold_percent": 4,
        "comparison": "same executable, different execution modes; no production code changed",
    }
    (output / "provenance.json").write_text(json.dumps(identity, indent=2) + "\n")
    verification = run([str(binary), "--mode", "verify"])
    (output / "verification.txt").write_text(verification)
    samples = []
    references = {}
    for round_index in range(2):
        for sample in range(10):
            order = modes if sample % 2 == 0 else tuple(reversed(modes))
            for case in cases:
                for mode in order:
                    command = [str(binary), "--mode", "native" if mode == "native-aa" else mode, "--case", case,
                               "--paths", str(args.paths)]
                    name = f"round{round_index + 1}-{sample:02d}-{case}-{mode}"
                    raw = run(["/usr/bin/time", "-f", "%M", "-o", str(output / f"{name}.rss-kib"), *command])
                    (output / f"{name}.json").write_text(raw)
                    result = json.loads(raw)
                    result["rss_kib"] = int((output / f"{name}.rss-kib").read_text())
                    if case in references:
                        compare(references[case], result)
                    else:
                        references[case] = result
                    result.update(round=round_index + 1, sample=sample, method=mode)
                    samples.append(result)
        print(f"round {round_index + 1} complete", flush=True)
    diagnostics = []
    for case in cases:
        for mode in modes[:-1]:
            raw = run([str(binary), "--mode", mode, "--case", case, "--paths", str(args.paths), "--phases", "true"])
            (output / f"diagnostic-{case}-{mode}.json").write_text(raw)
            value = json.loads(raw)
            compare(references[case], value)
            diagnostics.append(value)
    summaries = []
    for case in cases:
        minima = {
            mode: [min(s["seconds"] for s in samples if s["case"] == case and s["method"] == mode and s["round"] == r)
                   for r in (1, 2)] for mode in modes
        }
        deltas = {mode: [100.0 * (value / base - 1.0) for value, base in zip(values, minima["native"])]
                  for mode, values in minima.items()}
        aa_valid = all(abs(delta) < 4.0 for delta in deltas["native-aa"])
        replay = deltas["replay"]
        verdict = ("inconclusive" if not aa_valid else "slower" if all(d > 4.0 for d in replay)
                   else "faster" if all(d < -4.0 for d in replay) else "no-change")
        summaries.append({"case": case, "minima_seconds": minima, "delta_percent_vs_native": deltas,
                          "aa_valid": aa_valid, "replay_verdict": verdict})
    if digest(binary) != identity["binary_sha256"]:
        raise RuntimeError("executable changed during measurement")
    results = {"summaries": summaries, "diagnostics": diagnostics, "samples": samples}
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    (output / "load-after.txt").write_text(run(["ps", "-eo", "pid,pcpu,comm,args", "--sort=-pcpu"]))
    print(json.dumps(summaries, indent=2))


if __name__ == "__main__":
    main()

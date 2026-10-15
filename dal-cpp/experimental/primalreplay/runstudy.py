#!/usr/bin/env python3
"""Run the isolated replay experiment; invoke the entire script under the shared flock."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess  # nosec B404 -- executing the selected local study is this runner's purpose

MODES = ("native", "replay", "record", "caller", "native-aa")
CASES = ("vanilla", "barrier", "crossing")
RISKS = frozenset(("spot", "vol", "rate", "div", "STRIKE", "BARRIER"))
COUNTERS = ("hits", "recordings", "guard_failures")
MEMORY = ("peak_nodes", "graph_bytes", "graph_capacity")
PHASES = ("forward_seconds", "reverse_seconds", "hit_seconds", "miss_seconds")


class Runner:
    def __init__(self, binary):
        self.binary = binary.resolve()
        if self.binary.name != "primal_replay_study" or not self.binary.is_file() or not os.access(self.binary, os.X_OK):
            raise ValueError("expected an executable local primal_replay_study build")
        names = ("git", "c++", "cmake", "lscpu", "ps")
        self.tools = {name: Path(shutil.which(name)).resolve() for name in names}
        self.tools["time"] = Path("/usr/bin/time")
        self.allowed = frozenset((*self.tools.values(), self.binary))

    def capture(self, command):
        if not command or Path(command[0]) not in self.allowed:
            raise ValueError("executable is outside the study allowlist")
        # Only resolved tools or the operator's local build run; argv is never parsed by a shell.
        return subprocess.run(  # nosec B603  # nosemgrep
            command,  # nosemgrep
            check=True, text=True, capture_output=True, shell=False,
        ).stdout

    def tool(self, name, *arguments):
        return self.capture([str(self.tools[name]), *arguments])

    def study(self, mode, case="vanilla", paths=32768, phases=False, rss=None):
        if mode not in (*MODES[:-1], "verify") or case not in CASES:
            raise ValueError("unsupported study mode or case")
        if not 0 < paths <= 1000000:
            raise ValueError("path count is outside the study limit")
        command = [str(self.binary), "--mode", mode]
        if mode != "verify":
            command.extend(("--case", case, "--paths", str(paths)))
        if phases:
            command.extend(("--phases", "true"))
        if rss is not None:
            command = [str(self.tools["time"]), "-f", "%M", "-o", str(rss), *command]
        return self.capture(command)


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


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def read_object(raw, fields):
    value = json.loads(raw)
    require(type(value) is dict, "expected a JSON object")
    require(value.keys() == set(fields), "unexpected or missing output fields")
    return value


def finite_number(value, label, lower=-math.inf):
    require(type(value) in (int, float), f"{label} must be a number")
    try:
        finite = math.isfinite(value)
    except OverflowError:
        finite = False
    require(finite and value >= lower, f"invalid {label}: {value}")


def integer(value, label, lower=0):
    require(type(value) is int, f"{label} must be an integer")
    require(value >= lower, f"invalid {label}: {value}")


def validate_coordinates(result):
    finite_number(result["pv"], "pv")
    risks = result["risks"]
    require(type(risks) is dict, "risks must be an object")
    require(risks.keys() == RISKS, "expected the six named risks")
    for label, value in risks.items():
        finite_number(value, label)


def validate_counters(result, mode, paths):
    for field in COUNTERS:
        integer(result[field], field)
    hits, recordings, failures = (result[field] for field in COUNTERS)
    if mode == "replay":
        require(1 <= recordings <= paths, "replay needs an initial recording")
        require(hits + recordings == paths, "replay path counts disagree")
        require(failures == recordings - 1, "replay guard counts disagree")
    elif mode == "record":
        require((hits, recordings, failures) == (0, paths, 0), "forced recording counts disagree")
    else:
        require((hits, recordings, failures) == (0, 0, 0), "native/caller cannot report replay counts")


def validate_memory(result, mode, phases):
    for field in MEMORY:
        integer(result[field], field)
    require(result["graph_capacity"] >= result["graph_bytes"], "graph exceeds capacity")
    if phases or mode in ("replay", "record"):
        require(result["peak_nodes"] > 0, "missing graph nodes")
        require(result["graph_bytes"] >= result["peak_nodes"], "invalid occupied graph bytes")
    else:
        require(all(result[field] == 0 for field in MEMORY), "unexpected unmeasured native memory")


def validate_phases(result, mode, phases):
    for field in PHASES:
        finite_number(result[field], field, 0)
    forward, reverse, hit, miss = (result[field] for field in PHASES)
    require(forward + reverse <= result["seconds"], "phases exceed whole valuation time")
    if not phases or mode == "caller":
        require(all(result[field] == 0 for field in PHASES), "unexpected phase measurements")
    elif mode == "native":
        require(hit == miss == 0, "native cannot report replay phases")
    else:
        require(close(hit + miss, forward), "replay forward phases disagree")
        require(result["hits"] > 0 or hit == 0, "hit time without hits")


def validate_result(raw, mode, case, paths, phases=False):
    result = read_object(raw, ("mode", "case", "paths", "seconds", "pv", "risks", *COUNTERS, *MEMORY, *PHASES))
    expected_mode = "native" if mode == "native-aa" else mode
    require((result["mode"], result["case"]) == (expected_mode, case), "study mode/case differs from request")
    integer(result["paths"], "paths", 1)
    require(result["paths"] == paths, "study paths differ from request")
    finite_number(result["seconds"], "seconds", 0)
    require(result["seconds"] > 0, "whole valuation time must be positive")
    validate_coordinates(result)
    validate_counters(result, expected_mode, paths)
    validate_memory(result, expected_mode, phases)
    validate_phases(result, expected_mode, phases)
    return result


def verify_study(runner, output):
    raw = runner.study("verify")
    (output / "verification.txt").write_text(raw)
    fields = ("verification", "path_comparisons", "named_risks_per_path", "forced_guard_failures")
    result = read_object(raw, fields)
    require(result["verification"] == "passed", "study verification failed")
    for field in fields[1:]:
        integer(result[field], field, 1)
    require(result["named_risks_per_path"] == len(RISKS), "verification missed named risks")
    require(result["forced_guard_failures"] >= 2, "verification missed forced guard failures")


def read_options():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--paths", type=int, default=32768)
    parser.add_argument("--cpu", type=int, required=True)
    parser.add_argument("--cases", nargs="+", choices=CASES, default=CASES)
    return parser.parse_args()


def provenance(runner, repo, args):
    build = runner.binary.parent
    return {
        "source_head": runner.tool("git", "-C", str(repo), "rev-parse", "HEAD").strip(),
        "diff": runner.tool("git", "-C", str(repo), "diff", "HEAD"),
        "sources": {str(p.relative_to(repo)): digest(p) for p in Path(__file__).resolve().parent.iterdir() if p.is_file()},
        "production_tree": runner.tool("git", "-C", str(repo), "rev-parse", "HEAD:dal-cpp/dal").strip(),
        "binary_sha256": digest(runner.binary),
        "library_sha256": digest(build / "dal/dal-cpp/libdal_cpp.a"),
        "cache_sha256": digest(build / "CMakeCache.txt"),
        "flags": (build / "CMakeFiles/primal_replay_study.dir/flags.make").read_text(),
        "submodules": runner.tool("git", "-C", str(repo), "submodule", "status"),
        "compiler": runner.tool("c++", "--version"),
        "cmake": runner.tool("cmake", "--version"),
        "cpu": runner.tool("lscpu"),
        "load_before": runner.tool("ps", "-eo", "pid,pcpu,comm,args", "--sort=-pcpu"),
        "platform": platform.platform(),
        "affinity": sorted(os.sched_getaffinity(0)),
        "threads": 1,
        "paths": args.paths,
        "cases": args.cases,
        "samples_per_round": 10,
        "rounds": 2,
        "reduction": "min",
        "threshold_percent": 4,
        "comparison": "same executable, different execution modes; no production code changed",
    }


def retain_sample(runner, output, paths, case, mode, name, references):
    rss = output / f"{name}.rss-kib"
    raw = runner.study("native" if mode == "native-aa" else mode, case, paths, rss=rss)
    (output / f"{name}.json").write_text(raw)
    result = validate_result(raw, mode, case, paths)
    result["rss_kib"] = int(rss.read_text())
    integer(result["rss_kib"], "rss_kib", 1)
    if case in references:
        compare(references[case], result)
    else:
        references[case] = result
    return result


def collect_round(runner, output, args, round_index, references):
    samples = []
    for sample in range(10):
        order = MODES if sample % 2 == 0 else tuple(reversed(MODES))
        for case in args.cases:
            for mode in order:
                name = f"round{round_index + 1}-{sample:02d}-{case}-{mode}"
                result = retain_sample(runner, output, args.paths, case, mode, name, references)
                result.update(round=round_index + 1, sample=sample, method=mode)
                samples.append(result)
    return samples


def collect_diagnostics(runner, output, args, references):
    diagnostics = []
    for case in args.cases:
        for mode in MODES[:-1]:
            raw = runner.study(mode, case, args.paths, phases=True)
            (output / f"diagnostic-{case}-{mode}.json").write_text(raw)
            value = validate_result(raw, mode, case, args.paths, phases=True)
            compare(references[case], value)
            diagnostics.append(value)
    return diagnostics


def minimum(samples, case, mode, round_index):
    selected = (s["seconds"] for s in samples if s["case"] == case and s["method"] == mode and s["round"] == round_index)
    return min(selected)


def classify(deltas, aa_valid):
    if not aa_valid:
        return "inconclusive"
    if all(d > 4.0 for d in deltas):
        return "slower"
    if all(d < -4.0 for d in deltas):
        return "faster"
    return "no-change"


def summarize(samples, case):
    minima = {mode: [minimum(samples, case, mode, r) for r in (1, 2)] for mode in MODES}
    deltas = {mode: [100.0 * (value / base - 1.0) for value, base in zip(values, minima["native"])]
              for mode, values in minima.items()}
    aa_valid = all(abs(delta) < 4.0 for delta in deltas["native-aa"])
    return {"case": case, "minima_seconds": minima, "delta_percent_vs_native": deltas,
            "aa_valid": aa_valid, "replay_verdict": classify(deltas["replay"], aa_valid)}


def main():
    args = read_options()
    runner = Runner(args.binary)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parents[3]
    os.sched_setaffinity(0, {args.cpu})
    os.environ["DAL_NUM_THREADS"] = "1"
    identity = provenance(runner, repo, args)
    (output / "provenance.json").write_text(json.dumps(identity, indent=2) + "\n")
    verify_study(runner, output)
    samples = []
    references = {}
    for round_index in range(2):
        samples.extend(collect_round(runner, output, args, round_index, references))
        print(f"round {round_index + 1} complete", flush=True)
    diagnostics = collect_diagnostics(runner, output, args, references)
    summaries = [summarize(samples, case) for case in args.cases]
    if digest(runner.binary) != identity["binary_sha256"]:
        raise RuntimeError("executable changed during measurement")
    results = {"summaries": summaries, "diagnostics": diagnostics, "samples": samples}
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    (output / "load-after.txt").write_text(runner.tool("ps", "-eo", "pid,pcpu,comm,args", "--sort=-pcpu"))
    print(json.dumps(summaries, indent=2))


if __name__ == "__main__":
    main()

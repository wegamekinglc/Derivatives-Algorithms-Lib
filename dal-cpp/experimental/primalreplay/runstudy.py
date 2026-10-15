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
    result = json.loads(raw)
    result["rss_kib"] = int(rss.read_text())
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
            value = json.loads(raw)
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
    (output / "verification.txt").write_text(runner.study("verify"))
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

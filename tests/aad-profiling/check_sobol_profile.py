#!/usr/bin/env python3
"""Check selected Sobol profiling requests and complete-stream fingerprints."""

import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    if executable.name not in ("rng_perf", "rng_perf.exe") or tuple(p.name for p in executable.parents[:3]) != ("rng_perf", "benchmarks", "dal-cpp"):
        parser.error("expected the rng_perf build-tree executable")

    def run(arguments, success=True):
        result = subprocess.run([str(executable), "--sobol-profile", *arguments], capture_output=True, text=True, timeout=30, check=False)
        assert (result.returncode == 0) == success, (arguments, result.stdout, result.stderr)
        return [json.loads(line) for line in result.stdout.splitlines()] if success else []

    for dimension in (1, 52, 366):
        fingerprints = []
        for precision in ("uniform", "fast", "precise"):
            arguments = [str(dimension), "32", precision, "2"]
            rows = run(arguments)
            assert len(rows) == 2
            assert all(row["dimensions"] == dimension and row["paths"] == 32 and row["precision"] == precision and row["wall_ns"] > 0 for row in rows)
            assert rows[0]["fingerprint"] == rows[1]["fingerprint"] == run(arguments)[0]["fingerprint"]
            fingerprints.append(rows[0]["fingerprint"])
        assert len(set(fingerprints)) == 3
    for arguments in ([], ["0", "32", "fast", "1"], ["1", "0", "fast", "1"], ["1", "32", "fast", "0"],
                      ["-1", "32", "fast", "1"], ["1x", "32", "fast", "1"], ["1", "32", "unknown", "1"]):
        run(arguments, False)
    print("Sobol profile: deterministic full-stream fingerprints and invalid requests passed")


if __name__ == "__main__":
    main()

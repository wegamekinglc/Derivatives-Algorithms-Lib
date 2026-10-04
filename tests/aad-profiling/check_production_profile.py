#!/usr/bin/env python3
"""Exercise production profiling CLI contracts without timing thresholds."""

import argparse
import json
import math
import subprocess


def run(executable, arguments, success=True):
    result = subprocess.run([executable, "--production-profile", *arguments], capture_output=True, text=True, timeout=120)
    assert (result.returncode == 0) == success, (arguments, result.returncode, result.stdout, result.stderr)
    return [json.loads(line) for line in result.stdout.splitlines()] if success else []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--profiling", choices=("on", "off"), required=True)
    args = parser.parse_args()
    common = ["short", "32", "aad", "compiled"]
    cold = run(args.executable, [*common, "cold", "4", "0", "0", "1"])
    warm = run(args.executable, [*common, "warm", "4", "0", "0", "1"])
    for records in (cold, warm):
        request = next(row for row in records if row["record"] == "request")
        assert request["outputs"] == 4 and request["channel_width"] == 1
        assert request["paths_per_output"] == 32 and request["output_strategy"] == "sequential_single_output"
        results = [row for row in records if row["record"] == "result"]
        assert len(results) == 4 and len({row["output"] for row in results}) == 4
        assert len({row["pv"] for row in results}) == 4
        assert all(math.isfinite(row["pv"]) and all(map(math.isfinite, row["risks"])) for row in results)
    cold_results = [row for row in cold if row["record"] == "result"]
    warm_results = [row for row in warm if row["record"] == "result"]
    for lhs, rhs in zip(cold_results, warm_results):
        assert math.isclose(lhs["pv"], rhs["pv"], rel_tol=1e-10, abs_tol=1e-10)
        assert lhs["risk_names"] == rhs["risk_names"]
        assert all(math.isclose(x, y, rel_tol=1e-10, abs_tol=1e-10) for x, y in zip(lhs["risks"], rhs["risks"]))
    phases = run(args.executable, [*common, "phases", "1", "0", "0", "1"], args.profiling == "on")
    if phases:
        scopes = [row for row in phases if row["record"] == "scope"]
        assert scopes and all(row["complete"] for row in scopes)
        assert sum(row["tape_samples"] for row in scopes) > 32
        assert any(row["path_array_live_bytes"] > 0 for row in scopes)
        assert any(row["result_array_live_bytes"] > 0 for row in scopes)
        windows = [row for row in phases if row["record"] == "phase"]
        assert sum(row["calls"] for row in windows if row["phase"] == "REVERSE_SUFFIX") == 32
    for scenario in ("long", "local-vol", "lsmc-bs", "lsmc-local-vol"):
        records = run(args.executable, [scenario, "8", "aad", "compiled", "cold", "1",
                                       "2" if "vol" in scenario else "0", "64" if "lsmc" in scenario else "0", "1"])
        request = next(row for row in records if row["record"] == "request")
        result = next(row for row in records if row["record"] == "result")
        assert request["events"] > 1 and request["active_parameters"] == len(result["risks"])
        assert len(result["risk_names"]) == len(result["risks"])
    records = run(args.executable, ["short", "8", "double", "tree", "warm", "1", "0", "0", "1"])
    request = next(row for row in records if row["record"] == "request")
    result = next(row for row in records if row["record"] == "result")
    assert request["channel_width"] == 0 and request["active_parameters"] == 0
    assert result["risk_names"] == [] and result["risks"] == []
    for engine in ("tree", "compiled"):
        records = run(args.executable, ["short", "64", "aad", engine, "cold", "64", "0", "0", "1"])
        results = [row for row in records if row["record"] == "result"]
        assert len(results) == 64 and len({row["output"] for row in results}) == 64
        assert all(len(row["risks"]) == 4 for row in results)
    for bad in ([], [*common, "cold", "3", "0", "0", "1"], [*common, "cold", "1", "2", "0", "1"],
                ["short", "0", "aad", "compiled", "cold", "1", "0", "0", "1"]):
        run(args.executable, bad, False)
    print("production profile CLI contracts passed")


if __name__ == "__main__":
    main()

import argparse
import csv
import hashlib
import json
import os
import subprocess as sp
from pathlib import Path

from verdict import GROUPS, P1_FIELDS, judge_p1


def output(*command):
    return sp.check_output(command, text=True)


def hashes(paths):
    return {str(path): hashlib.file_digest(path.open("rb"), "sha256").hexdigest() for path in paths}


def write_csv(path, fields, rows):
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(fields)
        writer.writerows(rows)


def run_timing(run, out, rounds):
    for dupire in (False, True):
        shape_args = ["--dupire"] if dupire else []
        run("verify-dupire" if dupire else "verify-vanilla", "--s0", "--verify-only", *shape_args)
    rows = []
    for r in range(1, rounds + 1):
        (out / f"vmstat-before-{r}.txt").write_text(output("vmstat", "-n", "1", "3"))
        for p in range(1, 11):
            configurations = [("comparison", False), ("aa", False), ("comparison", True), ("aa", True)]
            offset = (r + p) % len(configurations)
            for kind, dupire in configurations[offset:] + configurations[:offset]:
                args = ["--s0-aa" if kind == "aa" else "--s0"]
                if dupire:
                    args.append("--dupire")
                if (r + p) % 2:
                    args.append("--centered-first")
                name = f"r{r}-p{p:02d}-{kind}-{'dupire' if dupire else 'vanilla'}"
                for row in csv.reader(run(name, *args).splitlines()):
                    if row and row[0] == "s0_pair":
                        _, k, case, directions, batch, first, calls, a_ns, b_ns = row
                        assert k == kind
                        rows.append((k, case, directions, r, p, batch, first, calls, a_ns, b_ns))
        (out / f"vmstat-after-{r}.txt").write_text(output("vmstat", "-n", "1", "3"))
        write_csv(out / "paired.csv", P1_FIELDS, rows)
        result = judge_p1([dict(zip(P1_FIELDS, row)) for row in rows], attempted_rounds=r)
        (out / "verdict.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"P1 completed round {r}: {result['verdict']}", flush=True)
        if r >= 2 and not result["needs_confirmation"]:
            break


def run_memory(run, out, processes=10):
    rows, header = [], None
    for case, directions in GROUPS:
        for route in ("dal", "xad"):
            for state in ("cold", "hot"):
                for p in range(1, processes + 1):
                    name = f"{case}-{directions}-{route}-{state}-p{p:02d}"
                    log = run(name, f"--s0-{route}-{state}", f"--s0-case={case}", f"--s0-directions={directions}")
                    lines = list(csv.reader(log.splitlines()))
                    columns, = [row[1:] for row in lines if row and row[0] == "# s0_memory"]
                    row, = [line[1:] for line in lines if line and line[0] == "s0_memory"]
                    assert row[:4] == [case, str(directions), route, state]
                    header = ("process", *columns)
                    cells = dict(zip(columns, row, strict=True))
                    peak = int(cells["peak"])
                    assert peak >= max(int(cells["pre_request_live"]), int(cells["result_live"]))
                    assert int(cells["result_live"]) > int(cells["post_result_live"])
                    assert int(cells["post_thread_live"]) == 0 and cells["allocs"] == cells["frees"]
                    if route == "xad":
                        assert int(cells["construct_aligned_bytes"]) == 268435456 and int(cells["construct_live"]) >= 268435456
                        assert int(cells["post_result_live"]) >= 268435456 and int(cells["post_backend_live"]) == 0
                    rows.append((p, *row))
    write_csv(out / "allocations.csv", header, rows)
    groups = {}
    for row in rows:
        cells = dict(zip(header, row, strict=True))
        key = tuple(cells[column] for column in ("case", "directions", "route", "state"))
        groups.setdefault(key, []).append(cells)
    metrics = ("construct_live", "construct_aligned_bytes", "pre_request_live", "peak", "result_live", "post_result_live", "post_backend_live", "post_thread_live", "largest_request",
               "aligned_allocs", "realloc_in_place", "realloc_moved", "realloc_failed", "cleanup_reserve", "logical_bytes", "maxrss_kib")
    def maximum(data, field):
        values = [int(row[field]) for row in data if row[field] != "unavailable"]
        return max(values) if values else "unavailable"

    write_csv(out / "allocation-maxima.csv", ("case", "directions", "route", "state", "processes", *metrics),
              ((*key, len(data), *(maximum(data, c) for c in metrics)) for key, data in groups.items()))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("timing", "memory"))
    parser.add_argument("build", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--cpu", type=int, default=4)
    parser.add_argument("--ledger", type=Path)
    args = parser.parse_args()
    build, out = args.build.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    logs = out / "runs"
    logs.mkdir()
    binary = build / "benchmarks/tape_perf/tape_perf"
    cache_text = (build / "CMakeCache.txt").read_text()
    config = {line.split(":", 1)[0]: line.split("=", 1)[1] for line in cache_text.splitlines() if line and line[0] not in "#/" and "=" in line}
    expected = {"CMAKE_BUILD_TYPE": "Release", "DAL_ENABLE_AAD_PROFILING": "OFF", "DAL_USE_EIGEN": "OFF", "XAD_SIMD_OPTION": "SSE2",
                "XAD_REDUCED_MEMORY": "OFF", "XAD_TAPE_REUSE_SLOTS": "OFF"}
    for name, value in expected.items():
        assert config[name] == value, (name, config[name], value)
    assert config["CMAKE_CXX_COMPILER_LAUNCHER"].endswith("/s0/fp-contract-off.sh"), "use the supplied FP compiler launcher"
    source, xad = config["CMAKE_HOME_DIRECTORY"], config["DAL_CPP_XAD_BENCHMARK_SOURCE"]
    assert not output("git", "-C", source, "status", "--porcelain")
    assert not output("git", "-C", xad, "status", "--porcelain")
    xad_commit = output("git", "-C", xad, "rev-parse", "HEAD").strip()
    assert xad_commit == "52d5513513286d67be081761761a2363060e5cd1"
    env = dict(os.environ, DAL_NUM_THREADS="1")
    env.pop("LD_PRELOAD", None)
    binaries = [binary, build / "libdal_cpp.so", build / "lib/libxad.a"]
    if args.mode == "memory":
        if args.ledger is None:
            parser.error("--ledger is required for memory")
        ledger = args.ledger.resolve(strict=True)
        env["LD_PRELOAD"] = str(ledger)
        binaries.append(ledger)
    (out / "CMakeCache.txt").write_text(cache_text)
    commit = output("git", "-C", source, "rev-parse", "HEAD").strip()
    (out / "environment.txt").write_text(
        f"core={commit}\nxad={xad_commit}\ncpu={args.cpu}\nDAL_NUM_THREADS=1\n"
        + output(config["CMAKE_CXX_COMPILER"], "--version") + output("cmake", "--version") + output("uname", "-srmo") + output("lscpu")
    )
    before = hashes(binaries)

    def run(name, *options):
        command = ["taskset", "-c", str(args.cpu), str(binary), "--second-order", *options]
        p = sp.run(command, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, text=True)
        (logs / f"{name}.csv").write_text(p.stdout)
        p.check_returncode()
        return p.stdout

    if args.mode == "timing":
        run_timing(run, out, 3)
    else:
        run_memory(run, out)
    after = hashes(binaries)
    assert after == before
    (out / "hashes.json").write_text(json.dumps({"before": before, "after": after}, indent=2) + "\n")


if __name__ == "__main__":
    main()

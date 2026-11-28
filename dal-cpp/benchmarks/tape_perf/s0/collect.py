import argparse
import csv
import hashlib
import json
import os
import subprocess as sp
from pathlib import Path

from verdict import GROUPS, P1_FIELDS, P3_FIELDS, duration, judge_p1, judge_p3


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
                    else:
                        limit = 256 * 1024 if int(cells["logical_bytes"]) <= 64 * 1024 else 8 * 1024 * 1024
                        assert int(cells["retained_tape"]) <= min(int(cells["post_result_live"]), limit)
                    rows.append((p, *row))
    write_csv(out / "allocations.csv", header, rows)
    groups = {}
    for row in rows:
        cells = dict(zip(header, row, strict=True))
        key = tuple(cells[column] for column in ("case", "directions", "route", "state"))
        groups.setdefault(key, []).append(cells)
    metrics = ("construct_live", "construct_aligned_bytes", "pre_request_live", "peak", "result_live", "post_result_live", "post_backend_live", "post_thread_live", "largest_request",
               "aligned_allocs", "realloc_in_place", "realloc_moved", "realloc_failed", "cleanup_reserve", "logical_bytes", "maxrss_kib",
               "tape_peak", "retained_tape")
    def maximum(data, field):
        values = [int(row[field]) for row in data if row[field] != "unavailable"]
        return max(values) if values else "unavailable"

    write_csv(out / "allocation-maxima.csv", ("case", "directions", "route", "state", "processes", *metrics),
              ((*key, len(data), *(maximum(data, c) for c in metrics)) for key, data in groups.items()))
    verdict = {}
    for case, directions in GROUPS:
        for state in ("cold", "hot"):
            dal = maximum(groups[case, str(directions), "dal", state], "peak")
            xad = maximum(groups[case, str(directions), "xad", state], "peak")
            verdict[f"{case}/{directions}/{state}"] = {"dal": dal, "xad": xad, "pass": 10 * dal <= 11 * xad + 40960}
    (out / "verdict.json").write_text(json.dumps(verdict, indent=2) + "\n")


def run_first_order(run, out, candidate, baseline):
    def measure(binary, name):
        minima = {}
        for shape, extra in (("vanilla", ()), ("dupire", ("--dupire",))):
            raw = run(f"{name}-{shape}", "--with-xad", *extra, executable=binary, mode="--first-order")
            for row in csv.reader(raw.splitlines()):
                if row and row[0] == "first_sample":
                    case, value = row[1], duration(row[6])
                    minima[case] = min(minima.get(case, value), value)
        return minima

    rows = []
    cases = {case for case, _ in GROUPS}
    for r in (1, 2):
        for p in range(1, 11):
            order = [("candidate", candidate), ("baseline", baseline)]
            if (r + p) % 2:
                order.reverse()
            results = {arm: measure(binary, f"r{r}-p{p:02d}-{arm}") for arm, binary in order}
            common = results["candidate"].keys() & results["baseline"].keys()
            for case in sorted(common):
                rows.append((case, r, p, order[0][0], results["candidate"][case], results["baseline"][case]))
    write_csv(out / "paired.csv", P3_FIELDS, rows)
    verdict = judge_p3([dict(zip(P3_FIELDS, row)) for row in rows], cases=cases)
    (out / "verdict.json").write_text(json.dumps(verdict, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("timing", "memory", "first-order"))
    parser.add_argument("build", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--cpu", type=int, default=4)
    parser.add_argument("--ledger", type=Path)
    parser.add_argument("--baseline", type=Path, help="independent 1860c950 Release core build for first-order replay")
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
    if args.mode == "first-order":
        if args.baseline is None:
            parser.error("--baseline is required for first-order")
        baseline = args.baseline.resolve(strict=True)
        baseline_binary = baseline / "benchmarks/tape_perf/tape_perf"
        baseline_cache = (baseline / "CMakeCache.txt").read_text()
        baseline_source, = [line.split("=", 1)[1] for line in baseline_cache.splitlines() if line.startswith("CMAKE_HOME_DIRECTORY:")]
        assert output("git", "-C", baseline_source, "rev-parse", "HEAD").strip() == "1860c950a2eba87bf5fd45faa357e06df340298a"
        assert not output("git", "-C", baseline_source, "status", "--porcelain")
        (out / "baseline-CMakeCache.txt").write_text(baseline_cache)
        binaries.extend((baseline_binary, baseline / "libdal_cpp.so", baseline / "lib/libxad.a"))
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

    def run(name, *options, executable=binary, mode="--second-order"):
        command = ["taskset", "-c", str(args.cpu), str(executable), mode, *options]
        p = sp.run(command, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, text=True)
        (logs / f"{name}.csv").write_text(p.stdout)
        p.check_returncode()
        return p.stdout

    if args.mode == "timing":
        run_timing(run, out, 3)
    elif args.mode == "memory":
        run_memory(run, out)
    else:
        run_first_order(run, out, binary, baseline_binary)
    after = hashes(binaries)
    assert after == before
    (out / "hashes.json").write_text(json.dumps({"before": before, "after": after}, indent=2) + "\n")


if __name__ == "__main__":
    main()

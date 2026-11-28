import argparse
import csv
import hashlib
import json
import os
import subprocess as sp  # nosec B404 -- local benchmark and environment probes
from itertools import product
from pathlib import Path

from verdict import GROUPS, P1_FIELDS, P3_FIELDS, duration, judge_p1, judge_p3


XAD_COMMIT = "52d5513513286d67be081761761a2363060e5cd1"
P3_BASELINE = "1860c950a2eba87bf5fd45faa357e06df340298a"
XAD_RESERVED_BYTES = 268435456
MEMORY_AXES = ("case", "directions", "route", "state")
MEMORY_METRICS = (
    "construct_live", "construct_aligned_bytes", "pre_request_live", "peak", "result_live", "post_result_live", "post_backend_live",
    "post_thread_live", "largest_request", "aligned_allocs", "realloc_in_place", "realloc_moved", "realloc_failed", "cleanup_reserve",
    "logical_bytes", "maxrss_kib", "tape_peak", "retained_tape",
)
BUILD_OPTIONS = {
    "CMAKE_BUILD_TYPE": "Release", "DAL_ENABLE_AAD_PROFILING": "OFF", "DAL_USE_EIGEN": "OFF",
    "XAD_SIMD_OPTION": "SSE2", "XAD_REDUCED_MEMORY": "OFF", "XAD_TAPE_REUSE_SLOTS": "OFF",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def output(*command):
    # Fixed diagnostic commands and the compiler from the caller's local CMake build.
    return sp.check_output(command, text=True, shell=False)  # nosec B603 # nosemgrep


def hashes(paths):
    result = {}
    for path in paths:
        with path.open("rb") as file:
            result[str(path)] = hashlib.file_digest(file, "sha256").hexdigest()
    return result


def write_csv(path, fields, rows):
    with path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(fields)
        writer.writerows(rows)


def write_json(path, result):
    path.write_text(json.dumps(result, indent=2) + "\n")


def timing_commands(round_number, process):
    configurations = [("comparison", False), ("aa", False), ("comparison", True), ("aa", True)]
    offset = (round_number + process) % len(configurations)
    for kind, dupire in configurations[offset:] + configurations[:offset]:
        args = ["--s0-aa" if kind == "aa" else "--s0"]
        if dupire:
            args.append("--dupire")
        if (round_number + process) % 2:
            args.append("--centered-first")
        shape = "dupire" if dupire else "vanilla"
        yield kind, f"r{round_number}-p{process:02d}-{kind}-{shape}", args


def timing_rows(run, round_number):
    for process in range(1, 11):
        for kind, name, args in timing_commands(round_number, process):
            for row in csv.reader(run(name, *args).splitlines()):
                if row and row[0] == "s0_pair":
                    _, reported_kind, case, directions, batch, first, calls, a_ns, b_ns = row
                    require(reported_kind == kind, f"{name}: expected {kind}, got {reported_kind}")
                    yield (reported_kind, case, directions, round_number, process, batch, first, calls, a_ns, b_ns)


def run_timing(run, out, rounds):
    run("verify-vanilla", "--s0", "--verify-only")
    run("verify-dupire", "--s0", "--verify-only", "--dupire")
    rows = []
    for r in range(1, rounds + 1):
        (out / f"vmstat-before-{r}.txt").write_text(output("vmstat", "-n", "1", "3"))
        rows.extend(timing_rows(run, r))
        (out / f"vmstat-after-{r}.txt").write_text(output("vmstat", "-n", "1", "3"))
        write_csv(out / "paired.csv", P1_FIELDS, rows)
        result = judge_p1([dict(zip(P1_FIELDS, row)) for row in rows], attempted_rounds=r)
        write_json(out / "verdict.json", result)
        print(f"P1 completed round {r}: {result['verdict']}", flush=True)
        if r >= 2 and not result["needs_confirmation"]:
            break


def validate_retention(cells):
    if cells["route"] == "xad":
        require(int(cells["construct_aligned_bytes"]) == XAD_RESERVED_BYTES, "unexpected XAD aligned reservation")
        require(int(cells["construct_live"]) >= XAD_RESERVED_BYTES, "XAD reservation missing at construction")
        require(int(cells["post_result_live"]) >= XAD_RESERVED_BYTES, "XAD reservation missing after result disposal")
        require(int(cells["post_backend_live"]) == 0, "XAD retained allocations after tape disposal")
        return
    large_limit = 16 if cells["logical_kind"] == "shared_logical_tape_peak" else 8
    limit = 256 * 1024 if int(cells["logical_bytes"]) <= 64 * 1024 else large_limit * 1024 * 1024
    require(int(cells["retained_tape"]) <= min(int(cells["post_result_live"]), limit), "DAL retained tape exceeds its limit")


def read_allocation(log, case, directions, route, state):
    lines = list(csv.reader(log.splitlines()))
    columns, = [row[1:] for row in lines if row and row[0] == "# s0_memory"]
    row, = [line[1:] for line in lines if line and line[0] == "s0_memory"]
    require(row[:4] == [case, str(directions), route, state], f"unexpected allocation row {row[:4]}")
    cells = dict(zip(columns, row, strict=True))
    peak = int(cells["peak"])
    require(peak >= max(int(cells["pre_request_live"]), int(cells["result_live"])), "peak excludes live request bytes")
    require(int(cells["result_live"]) > int(cells["post_result_live"]), "result disposal did not release its allocations")
    require(int(cells["post_thread_live"]) == 0, "allocations retained after measured thread exit")
    require(cells["allocs"] == cells["frees"], "allocation and free counts differ")
    validate_retention(cells)
    return columns, row


def maximum(rows, field):
    values = [int(row[field]) for row in rows if row[field] != "unavailable"]
    return max(values) if values else "unavailable"


def memory_verdict(groups):
    verdict = {}
    for (case, directions), state in product(GROUPS, ("cold", "hot")):
        dal = maximum(groups[case, str(directions), "dal", state], "peak")
        xad = maximum(groups[case, str(directions), "xad", state], "peak")
        verdict[f"{case}/{directions}/{state}"] = {"dal": dal, "xad": xad, "pass": 10 * dal <= 11 * xad + 40960}
    return verdict


def report_memory(out, header, rows):
    write_csv(out / "allocations.csv", header, rows)
    groups = {}
    for row in rows:
        cells = dict(zip(header, row, strict=True))
        key = tuple(cells[column] for column in MEMORY_AXES)
        groups.setdefault(key, []).append(cells)
    write_csv(out / "allocation-maxima.csv", (*MEMORY_AXES, "processes", *MEMORY_METRICS),
              ((*key, len(data), *(maximum(data, c) for c in MEMORY_METRICS)) for key, data in groups.items()))
    write_json(out / "verdict.json", memory_verdict(groups))


def run_memory(run, out, processes=10):
    rows, header = [], None
    for (case, directions), route, state, p in product(GROUPS, ("dal", "xad"), ("cold", "hot"), range(1, processes + 1)):
        name = f"{case}-{directions}-{route}-{state}-p{p:02d}"
        log = run(name, f"--s0-{route}-{state}", f"--s0-case={case}", f"--s0-directions={directions}")
        columns, row = read_allocation(log, case, directions, route, state)
        row_header = ("process", *columns)
        require(header is None or header == row_header, f"{name}: allocation column layout changed")
        header = row_header
        rows.append((p, *row))
    report_memory(out, header, rows)


def first_order_minima(run, binary, name):
    minima = {}
    for shape, extra in (("vanilla", ()), ("dupire", ("--dupire",))):
        log = run(f"{name}-{shape}", "--with-xad", *extra, executable=binary, mode="--first-order")
        for row in csv.reader(log.splitlines()):
            if row and row[0] == "first_sample":
                case, value = row[1], duration(row[6])
                minima[case] = min(minima.get(case, value), value)
    return minima


def run_first_order(run, out, candidate, baseline):
    rows = []
    for r, p in product((1, 2), range(1, 11)):
        order = [("candidate", candidate), ("baseline", baseline)]
        if (r + p) % 2:
            order.reverse()
        results = {arm: first_order_minima(run, binary, f"r{r}-p{p:02d}-{arm}") for arm, binary in order}
        common = results["candidate"].keys() & results["baseline"].keys()
        for case in sorted(common):
            rows.append((case, r, p, order[0][0], results["candidate"][case], results["baseline"][case]))
    write_csv(out / "paired.csv", P3_FIELDS, rows)
    cases = {case for case, _ in GROUPS}
    write_json(out / "verdict.json", judge_p3([dict(zip(P3_FIELDS, row)) for row in rows], cases=cases))


def build_cache(build):
    text = (build / "CMakeCache.txt").read_text()
    values = {}
    for line in text.splitlines():
        if line and line[0] not in "#/" and "=" in line:
            key, value = line.split("=", 1)
            values[key.split(":", 1)[0]] = value
    return text, values


def check_revision(source, expected=None):
    commit = output("git", "-C", source, "rev-parse", "HEAD").strip()
    require(not output("git", "-C", source, "status", "--porcelain"), f"source must be clean: {source}")
    require(expected is None or commit == expected, f"expected {expected}, got {commit}: {source}")
    return commit


def binary_files(build):
    return [build / "benchmarks/tape_perf/tape_perf", build / "libdal_cpp.so", build / "lib/libxad.a"]


def prepare_candidate(build, out, cpu, shared):
    cache, config = build_cache(build)
    for name, value in BUILD_OPTIONS.items():
        require(config.get(name) == value, f"{name} must be {value}, got {config.get(name)}")
    require(config["CMAKE_CXX_COMPILER_LAUNCHER"].endswith("/s0/fp-contract-off.sh"), "use the supplied FP compiler launcher")
    commit = check_revision(config["CMAKE_HOME_DIRECTORY"])
    xad = check_revision(config["DAL_CPP_XAD_BENCHMARK_SOURCE"], XAD_COMMIT)
    (out / "CMakeCache.txt").write_text(cache)
    (out / "environment.txt").write_text(
        f"core={commit}\nxad={xad}\ncpu={cpu}\nDAL_NUM_THREADS=1\nshared={shared}\n"
        + output(config["CMAKE_CXX_COMPILER"], "--version") + output("cmake", "--version") + output("uname", "-srmo") + output("lscpu")
    )
    return binary_files(build)


def prepare_baseline(build, out):
    baseline = build.resolve(strict=True)
    cache, config = build_cache(baseline)
    check_revision(config["CMAKE_HOME_DIRECTORY"], P3_BASELINE)
    (out / "baseline-CMakeCache.txt").write_text(cache)
    return binary_files(baseline)


def launcher(binary, logs, cpu, shared, env):
    shared_args = ["--s0-shared"] if shared else []

    def run(name, *options, executable=binary, mode="--second-order"):
        command = ["taskset", "-c", str(cpu), str(executable), mode, *shared_args, *options]
        # Use the caller's built executable and fixed benchmark arguments without a shell.
        process = sp.run(command, env=env, stdout=sp.PIPE, stderr=sp.STDOUT, text=True, shell=False)  # nosec B603 # nosemgrep
        (logs / f"{name}.csv").write_text(process.stdout)
        process.check_returncode()
        return process.stdout

    return run


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("timing", "memory", "first-order"))
    parser.add_argument("build", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--cpu", type=int, default=4)
    parser.add_argument("--ledger", type=Path, help="trusted local glibc interposer for memory mode")
    parser.add_argument("--baseline", type=Path, help="independent 1860c950 Release core build for first-order replay")
    parser.add_argument("--shared", action="store_true", help="use explicit sharing for the qualified basket/6 and Dupire/3 S0 requests")
    args = parser.parse_args()
    if args.shared and args.mode == "first-order":
        parser.error("--shared applies to second-order timing and memory")
    if args.mode == "first-order" and args.baseline is None:
        parser.error("--baseline is required for first-order")
    if args.mode == "memory" and args.ledger is None:
        parser.error("--ledger is required for memory")
    return args


def main():
    args = parse_args()
    out = args.out.resolve()
    logs = out / "runs"
    logs.mkdir(parents=True, exist_ok=False)
    binaries = prepare_candidate(args.build.resolve(), out, args.cpu, args.shared)
    binary = binaries[0]
    env = dict(os.environ, DAL_NUM_THREADS="1")
    env.pop("LD_PRELOAD", None)
    if args.mode == "first-order":
        baseline = prepare_baseline(args.baseline, out)
        binaries.extend(baseline)
    if args.mode == "memory":
        ledger = args.ledger.resolve(strict=True)
        env["LD_PRELOAD"] = str(ledger)
        binaries.append(ledger)
    before = hashes(binaries)
    run = launcher(binary, logs, args.cpu, args.shared, env)
    if args.mode == "timing":
        run_timing(run, out, 3)
    elif args.mode == "memory":
        run_memory(run, out)
    else:
        run_first_order(run, out, binary, baseline[0])
    after = hashes(binaries)
    require(after == before, "benchmark artifacts changed during sampling")
    write_json(out / "hashes.json", {"before": before, "after": after})


if __name__ == "__main__":
    main()

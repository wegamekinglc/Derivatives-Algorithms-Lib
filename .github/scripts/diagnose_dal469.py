#!/usr/bin/env python3
"""Private manual-only diagnostics for the fixed DAL-469 comparison."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile

import check_benchmark_regressions as native_gate
import check_python_benchmark_regressions as python_gate
from write_benchmark_metadata import command_version, cpu_model


BASE_SHA = "72b3d15e04a9a42859b3f3b264b0f4ea635ddb20"
HEAD_SHA = "d7eaefb7808ba32aecc870b7babc7cc9e7fc650a"
SCRIPTS = Path(__file__).resolve().parent
ORIGINAL_HASHES = {
    "base": "2158de6b9629241d15813869f4c7d323fd1f1a6d19924c30eae183be520989a6",
    "head": "ef6e0f629696afbbe9c45447ef1a4c6bb474a9683effbe10fbd95459341caaf1",
}
REQUIRED_ISA = {"avx2", "avx512f", "avx512bw", "avx512dq", "avx512vl"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def literal_argv(argv):
    require(isinstance(argv, (list, tuple)) and bool(argv), "command requires a nonempty literal argv list")
    require(all(isinstance(arg, (str, Path)) for arg in argv) and bool(str(argv[0])),
            "command requires nonempty executable and string/path literal argv elements")
    return [str(arg) for arg in argv]


def run_command(argv, log, *, env=None, timeout=600):
    argv = literal_argv(argv)
    log.parent.mkdir(parents=True, exist_ok=True)
    try:
        with log.open("w", encoding="utf-8") as stream:
            # Audited fixed callers; validated argv and shell=False keep metacharacters literal.
            status = subprocess.run(  # nosemgrep: dangerous-subprocess-use-audit
                argv, cwd=log.parent, env=env, shell=False, check=False,
                stdout=stream, stderr=subprocess.STDOUT, timeout=timeout,
            ).returncode
    except (OSError, subprocess.TimeoutExpired) as error:
        status = 124 if isinstance(error, subprocess.TimeoutExpired) else 127
        with log.open("a", encoding="utf-8") as stream:
            stream.write(f"{type(error).__name__}: {error}\n")
    write_json(log.with_suffix(".command.json"), {"argv": argv, "returncode": status})
    return status


def layout(workspace):
    head = workspace / "diagnostic-workspace"
    base = head / "benchmark-base"
    return {
        "head": {"source": head, "build": head / "build/benchmark-head"},
        "base": {"source": base, "build": base / "build/benchmark-base"},
    }


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def native_module(package):
    modules = list((package / "dal").glob("_dal.*.so"))
    require(len(modules) == 1, f"expected exactly one native module in {package}")
    require(modules[0].resolve().is_relative_to(package.resolve()), "native module escapes package")
    return modules[0]


def unpack_package(archive, destination):
    require(not destination.exists(), f"package destination already exists: {destination}")
    with tarfile.open(archive) as stream:
        members = stream.getmembers()
        for member in members:
            name = Path(member.name)
            require(bool(name.parts) and not name.is_absolute() and ".." not in name.parts and name.parts[0] == "dal"
                    and (member.isfile() or member.isdir()), f"unsafe archive member: {member.name}")
        stream.extractall(destination, members=members, filter="data")


def prefix_command(suite, package, output):
    return [sys.executable, "-s", SCRIPTS / "dal469_prefix_worker.py",
            "--suite", suite, "--package", package, "--output", output]


def worker_environment():
    return {**os.environ, "PYTHONPATH": "", "PYTHONNOUSERSITE": "1",
            "PYTHONDONTWRITEBYTECODE": "1", "DAL_NUM_THREADS": "4"}


def verified_original_packages(original, output):
    packages = {}
    for side in ("base", "head"):
        package = output / "original-packages" / side
        unpack_package(original / f"{side}-package.tar.gz", package)
        require(sha256(native_module(package)) == ORIGINAL_HASHES[side], f"original {side} module hash mismatch")
        packages[side] = str(package)
    return packages


def probe_original_packages(suite, packages, output):
    probes = {}
    for side in ("base", "head"):
        probe = output / "original-probe" / side
        probes[side] = run_command(prefix_command(suite, packages[side], probe),
                                   probe / "worker.log", env=worker_environment())
        require(probes[side] in (0, -4, 78), f"original {side} correctness/probe failed: {probes[side]}")
    return probes


def choose_packages(workspace, output, flags, python_version):
    builds = layout(workspace)
    fallback = {"python_mode": "rebuild", "packages": {
        side: str(info["build"] / "dal-python") for side, info in builds.items()
    }}
    if not REQUIRED_ISA.issubset(flags):
        return {**fallback, "reason": "Original AVX-512 ISA prerequisites are unavailable"}
    if tuple(python_version) != (3, 13, 17):
        return {**fallback, "reason": "Original Python 3.13.17 is unavailable"}
    original = Path(os.environ.get("RUNNER_TEMP", workspace)) / "dal469-original/python-reproduction"
    if not all((original / f"{side}-package.tar.gz").is_file() for side in builds):
        return {**fallback, "reason": "Original artifact unavailable; both sides will be rebuilt"}
    packages = verified_original_packages(original, output)
    probes = probe_original_packages(builds["head"]["source"] / "dal-python/benchmarks", packages, output)
    if any(probes.values()):
        return {**fallback, "reason": "Original binary cannot execute (SIGILL or import/ABI failure)",
                "original_probe_status": probes}
    return {"python_mode": "original", "packages": packages, "original_probe_status": probes,
            "reason": "Original module hashes and complete eight-case ISA/correctness probes passed"}


def source_identity(source, expected):
    def git(*args):
        return subprocess.run(["git", "-C", str(source), *args], check=True,
                              capture_output=True, text=True, timeout=30).stdout.strip()
    sha = git("rev-parse", "HEAD")
    status = git("status", "--porcelain")
    require(sha == expected and not status, f"unexpected or dirty diagnostic source: {source}")
    dependencies = git("submodule", "status", "--recursive")
    require(not any(line.startswith(("-", "+", "U")) for line in dependencies.splitlines()),
            f"unpinned or missing diagnostic dependencies: {source}")
    return {"sha": sha, "tree": git("rev-parse", "HEAD^{tree}"), "status": status,
            "dependencies": dependencies, "source": str(source)}


def environment_differences(environment):
    expected = {"cpu": "AMD EPYC 9V74 80-Core Processor", "gcc": "14.2.0", "cmake": "3.31.6", "python": "3.13.17"}
    differences = {
        key: {"expected": value, "actual": environment[key]}
        for key, value in expected.items() if value not in environment[key]
    }
    if environment["cpu_count"] != 4:
        differences["cpu_count"] = {"expected": 4, "actual": environment["cpu_count"]}
    return differences


def diagnostic_environment():
    flags = next((set(line.split(":", 1)[1].split()) for line in Path("/proc/cpuinfo").read_text().splitlines()
                  if line.startswith("flags")), set())
    environment = {
        "cpu": cpu_model(), "isa": sorted(flags), "cpu_count": os.cpu_count(), "platform": platform.platform(),
        "gcc": command_version("gcc-14"), "cmake": command_version("cmake"), "python": sys.version,
        "runner_image": f"{os.getenv('ImageOS', 'unknown')}/{os.getenv('ImageVersion', 'unknown')}",
        "controller_sha": os.getenv("GITHUB_SHA"), "controller_ref": os.getenv("GITHUB_REF"),
    }
    environment["differences_from_original"] = environment_differences(environment)
    environment["dal_num_threads"] = 4
    environment["original_ci_exit_status"] = 1
    environment["original_ci_run"] = 40411137529
    environment["original_module_sha256"] = ORIGINAL_HASHES
    return environment


def copy_original_evidence(workspace, output):
    original = Path(os.environ.get("RUNNER_TEMP", workspace)) / "dal469-original"
    for name in ("environment.json", "python-paired/results.json"):
        if (original / name).is_file():
            destination = output / "original-evidence" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original / name, destination)


def prepare(workspace, output):
    environment = diagnostic_environment()
    write_json(output / "environment.json", environment)
    run_command(["lscpu", "--json"], output / "lscpu.json")
    copy_original_evidence(workspace, output)
    builds = layout(workspace)
    sources = {side: source_identity(info["source"], BASE_SHA if side == "base" else HEAD_SHA)
               for side, info in builds.items()}
    write_json(output / "sources.json", sources)
    selection = choose_packages(workspace, output, set(environment["isa"]), sys.version_info[:3])
    write_json(output / "selection.json", selection)
    if os.getenv("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as stream:
            stream.write(f"python_mode={selection['python_mode']}\n")
    return 0


def run_tape(builds, output):
    return run_command([
        sys.executable, SCRIPTS / "check_benchmark_regressions.py",
        "--base-root", builds["base"]["build"], "--head-root", builds["head"]["build"],
        "--output-dir", output / "tape", "--benchmarks", "tape_perf",
        "--samples", "10", "--confirmation-rounds", "2", "--threshold-percent", "4",
        "--retry-identical-binaries",
    ], output / "tape-gate.log", env=worker_environment(), timeout=3600)


def run_prefix(builds, packages, output):
    result = {"schema": "dal.python-benchmark-diagnostic/1", "status": "running", "samples": 10,
              "rounds": 2, "threshold_percent": 4, "comparisons": [], "failures": []}
    collected = {"base": [], "head": []}
    suite = builds["head"]["source"] / "dal-python/benchmarks"
    try:
        for index in range(20):
            for side in (("base", "head") if index % 2 == 0 else ("head", "base")):
                destination = output / "raw" / f"{index + 1:02d}-{side}"
                status = run_command(prefix_command(suite, packages[side], destination),
                                     destination / "worker.log", env=worker_environment())
                require(status == 0, f"prefix {index + 1}/{side} exited {status}")
                report = json.loads((destination / "results.json").read_text())
                require(report["environment"]["native_sha256"] == sha256(native_module(packages[side])),
                        f"reported {side} native identity differs from selected module")
                collected[side].append(report)
        result["comparisons"], result["failures"] = python_gate.evaluate(
            collected["base"], collected["head"], 10, 2, 4)
        result["raw_samples_ns"] = python_gate.comparable_samples(collected["base"], collected["head"])
        result["environments"] = {side: reports[0]["environment"] for side, reports in collected.items()}
    except (OSError, ValueError, KeyError, TypeError, RuntimeError) as error:
        result["failures"].append(f"{type(error).__name__}: {error}")
    result["status"] = "failed" if result["failures"] else "passed"
    python_gate.write_report(output, result)
    summary = output / "summary.md"
    summary.write_text(summary.read_text().replace(
        "Both native builds run the same head benchmark suite at full scale. Every current case is compared, including on first introduction.",
        "Scoped diagnostic: only the first eight full-scale head-suite cases, in original order. This is not the scheduled full-suite gate.",
    ), encoding="utf-8")
    return int(bool(result["failures"]))


def measure_targets(output, builds, packages):
    statuses = {"original_ci_exit_status": 1}
    for name, operation in (("tape", lambda: run_tape(builds, output)),
                            ("python-prefix", lambda: run_prefix(builds, packages, output / "python-prefix"))):
        try:
            statuses[name] = operation()
        except (OSError, ValueError, RuntimeError) as error:
            statuses[name] = 1
            write_json(output / f"{name}-error.json", {"error": str(error)})
        write_json(output / "measurement-status.json", statuses)
    return next((statuses[name] for name in ("tape", "python-prefix") if statuses[name]), 0)


def identities(builds, packages):
    configurations, identities = {}, {}
    for side, info in builds.items():
        source = source_identity(info["source"], BASE_SHA if side == "base" else HEAD_SHA)
        configurations[side] = python_gate.build_configuration(info["build"], info["source"])
        require(native_gate.static_build(info["build"]), "diagnostic requires static DAL builds")
        identities[side] = {"source": source, "configuration": configurations[side],
                            "tape_sha256": sha256(native_gate.benchmark_binary(info["build"], "tape_perf")),
                            "python_sha256": sha256(native_module(packages[side]))}
    require(not python_gate.configuration_drift(configurations["base"], configurations["head"]),
            "diagnostic base/head configuration drift")
    return identities


def preserve_replay_evidence(builds, packages, output):
    for side, info in builds.items():
        shutil.copy2(info["build"] / "CMakeCache.txt", output / f"{side}-CMakeCache.txt")
        commands = info["build"] / "compile_commands.json"
        if commands.is_file():
            shutil.copy2(commands, output / f"{side}-compile_commands.json")
        binaries = output / "binaries" / side
        binaries.mkdir(parents=True, exist_ok=True)
        shutil.copy2(native_gate.benchmark_binary(info["build"], "tape_perf"), binaries / "tape_perf")
        with tarfile.open(binaries / "python-package.tar.gz", "w:gz") as archive:
            archive.add(packages[side] / "dal", arcname="dal")


def numerical_preflights(builds, packages, mode, output):
    for side, info in builds.items():
        require(run_command([native_gate.benchmark_binary(info["build"], "tape_perf"), "--diagnostics"],
                            output / "preflight" / f"{side}-tape.log", env=worker_environment()) == 0,
                f"{side} tape correctness failed")
        if mode == "rebuild":
            destination = output / "preflight" / f"{side}-prefix"
            require(run_command(prefix_command(builds["head"]["source"] / "dal-python/benchmarks",
                                               packages[side], destination), destination / "worker.log",
                                env=worker_environment()) == 0, f"{side} prefix correctness failed")


def append_measurement_summary(output, mode, status):
    if os.getenv("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as stream:
            stream.write(f"## DAL-469 diagnostic\n\nMode: {mode}. Sampler exit: {status}. "
                         "Original CI remains failed; a diagnostic pass does not clear it.\n\n")
            for summary in (output / "tape/summary.md", output / "python-prefix/summary.md"):
                if summary.exists():
                    stream.write(summary.read_text() + "\n")


def measure(workspace, output):
    builds = layout(workspace)
    selection = json.loads((output / "selection.json").read_text())
    packages = {side: Path(package) for side, package in selection["packages"].items()}
    preserve_replay_evidence(builds, packages, output)
    before = identities(builds, packages)
    write_json(output / "identities-before.json", before)
    if selection["python_mode"] == "original":
        require(all(before[side]["python_sha256"] == ORIGINAL_HASHES[side] for side in builds),
                "original package changed after compatibility probe")
    numerical_preflights(builds, packages, selection["python_mode"], output)
    status = measure_targets(output, builds, packages)
    after = identities(builds, packages)
    write_json(output / "identities-after.json", after)
    require(before == after, "diagnostic source/configuration/binary identity changed during sampling")
    append_measurement_summary(output, selection["python_mode"], status)
    return status


def profile_sigill(builds, selection, output):
    statuses = {}
    for side, status in selection.get("original_probe_status", {}).items():
        if status == -4 and shutil.which("gdb"):
            destination = output / "profiles" / side / "original-sigill"
            command = prefix_command(builds["head"]["source"] / "dal-python/benchmarks",
                                     output / "original-packages" / side, destination / "prefix-output")
            statuses[f"{side}-original-sigill"] = run_command([
                "gdb", "--batch", "-ex", "set pagination off", "-ex", "run",
                "-ex", "x/12i $pc-16", "-ex", "thread apply all bt 8", "--args", *command,
            ], destination / "gdb.txt", env=worker_environment())
    return statuses


def profile_command(command, destination):
    statuses = {"resources": run_command(
        ["/usr/bin/time", "--verbose", *command], destination / "resources.txt", env=worker_environment())}
    if shutil.which("perf"):
        status = run_command(["perf", "record", "-o", destination / "perf.data", "-g", "--", *command],
                             destination / "perf-record.txt", env=worker_environment())
        statuses["perf"] = status
        if status == 0:
            run_command(["perf", "report", "--stdio", "-i", destination / "perf.data"], destination / "perf-report.txt")
    else:
        statuses["perf"] = "unavailable"
    allocator = Path("/usr/lib/x86_64-linux-gnu/libmemusage.so")
    if allocator.is_file():
        statuses["allocations"] = run_command(command, destination / "allocations.txt", env={
            **worker_environment(), "LD_PRELOAD": str(allocator), "MEMUSAGE_OUTPUT": str(destination / "memusage.data"),
        })
    else:
        statuses["allocations"] = "libmemusage unavailable"
    return statuses


def profile(workspace, output):
    builds = layout(workspace)
    selection = json.loads((output / "selection.json").read_text())
    statuses = profile_sigill(builds, selection, output)
    for side, info in builds.items():
        commands = {
            "tape": [native_gate.benchmark_binary_path(info["build"], "tape_perf")],
            "prefix": prefix_command(builds["head"]["source"] / "dal-python/benchmarks",
                                     selection["packages"][side], output / "profiles" / side / "prefix-output"),
        }
        for name, command in commands.items():
            destination = output / "profiles" / side / name
            statuses.update({f"{side}-{name}-{key}": value for key, value in profile_command(command, destination).items()})
    write_json(output / "profiles/status.json", statuses)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=("prepare", "measure", "profile"))
    args = parser.parse_args(argv)
    require(os.environ.get("GITHUB_EVENT_NAME") == "workflow_dispatch"
            and os.environ.get("DAL469_MODE") == "dal-469", "requires manual dal-469 mode")
    workspace = Path.cwd().resolve()
    output = workspace / "diagnostic-results"
    output.mkdir(exist_ok=True)
    try:
        return {"prepare": prepare, "measure": measure, "profile": profile}[args.stage](workspace, output)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        write_json(output / f"{args.stage}-error.json", {"error": f"{type(error).__name__}: {error}",
                                                       "original_ci_exit_status": 1})
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

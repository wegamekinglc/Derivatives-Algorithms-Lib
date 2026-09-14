"""Fixed-budget replay of archived DAL-201 binaries; never a regression gate."""

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import shutil
import signal
import subprocess
import sys
import sysconfig
import tarfile
import time


HERE = Path(__file__).resolve().parent
REPOSITORY = "wegamekinglc/Derivatives-Algorithms-Lib"
ABI = "cpython-313-x86_64-linux-gnu"
REQUIRED_FLAGS = ("avx", "avx2", "avx512f", "avx512vl", "avx512dq", "avx512bw")
TARGETS = ("mc.vanilla.aad.compiled", "xccy.joint.ANALYTIC.diagnostics", "xccy.joint.ANALYTIC.solve")
ARMS = ("base", "head", "base-copy", "head-copy")
SAMPLES = 20
DEADLINE = None
CANCEL_SIGNAL = None
CANCEL_DEPTH = 0


class HarnessInterrupted(KeyboardInterrupt):
    def __init__(self, signum):
        self.signum = signum
        super().__init__(signal.Signals(signum).name)


@contextmanager
def cancellation_scope():
    global CANCEL_DEPTH, CANCEL_SIGNAL
    previous = {}
    if CANCEL_DEPTH == 0:
        CANCEL_SIGNAL = None
        def request_cancel(signum, frame):
            global CANCEL_SIGNAL
            if CANCEL_SIGNAL is None:
                CANCEL_SIGNAL = signum
        for signum in (signal.SIGINT, signal.SIGTERM):
            previous[signum] = signal.signal(signum, request_cancel)
    CANCEL_DEPTH += 1
    try:
        yield
    finally:
        CANCEL_DEPTH -= 1
        for signum, handler in previous.items():
            signal.signal(signum, handler)


def check_interruption():
    if CANCEL_SIGNAL is not None:
        raise HarnessInterrupted(CANCEL_SIGNAL)


def wait_process(process, timeout):
    until = time.monotonic() + timeout
    while True:
        check_interruption()
        # Keep the leader unreaped until killpg, preventing process-group ID reuse.
        if os.waitid(os.P_PID, process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT):
            return
        if time.monotonic() >= until:
            raise subprocess.TimeoutExpired(process.args, timeout)
        time.sleep(min(0.05, max(0, until - time.monotonic())))


def live_group_members(group):
    members = []
    for path in Path("/proc").glob("[0-9]*/stat"):
        try:
            fields = path.read_text().rsplit(")", 1)[1].split()
        except (FileNotFoundError, ProcessLookupError):
            continue
        if int(fields[2]) == group and fields[0] not in ("Z", "X"):
            members.append(int(path.parent.name))
    return members


def collect_process_group(process):
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait(timeout=5)
    until = time.monotonic() + 2
    while members := live_group_members(process.pid):
        if time.monotonic() >= until:
            raise RuntimeError(f"process group did not stop: {members}")
        time.sleep(0.01)


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def sha256(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def verify_hash(path, expected):
    actual = sha256(path)
    if actual != expected:
        raise ValueError(f"SHA256 mismatch: {path.name}: {actual} != {expected}")
    return actual


def extract_package(archive, destination):
    with tarfile.open(archive, "r:gz") as source:
        members = source.getmembers()
        names = set()
        if sum(member.size for member in members) > 512 * 1024**2:
            raise ValueError("package exceeds expanded size budget")
        for member in members:
            path = Path(member.name)
            if (path.is_absolute() or ".." in path.parts or not path.parts
                    or path.parts[0] != "dal" or not (member.isfile() or member.isdir())
                    or member.name in names):
                raise ValueError(f"unsafe archive member: {member.name}")
            names.add(member.name)
        destination.mkdir(parents=True, exist_ok=False)
        source.extractall(destination, members=members, filter="data")


def require_runtime(flags, abi):
    if abi != ABI:
        raise ValueError(f"Python ABI mismatch: {abi}; require {ABI}")
    missing = set(REQUIRED_FLAGS) - set(flags)
    if missing:
        raise ValueError(f"missing CPU ISA: {', '.join(sorted(missing))}")


def clean_environment():
    env = os.environ.copy()
    for key in ("PYTHONPATH", "PYTHONHOME", "LD_PRELOAD", "LD_LIBRARY_PATH"):
        env.pop(key, None)
    env.update(DAL_NUM_THREADS="4", PYTHONNOUSERSITE="1")
    return env


@cancellation_scope()
def run_process(command, output, *, timeout, env=None, cwd=None):
    check_interruption()
    if DEADLINE is not None:
        timeout = min(timeout, DEADLINE - time.monotonic())
        if timeout <= 0:
            raise ValueError("fixed 30-minute execution budget exhausted")
    output.mkdir(parents=True, exist_ok=False)
    record = {"command": [str(part) for part in command], "status": "starting", "timeout_seconds": timeout,
              "returncode": None}
    record["cwd"] = str(cwd) if cwd else None
    record["environment"] = {key: (env or os.environ).get(key) for key in (
        "DAL_NUM_THREADS", "OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS", "PYTHONNOUSERSITE", "LD_PRELOAD")}
    write_json(output / "process.json", record)
    start = time.monotonic()
    process = None
    try:
        with (output / "stdout.txt").open("wb") as stdout, (output / "stderr.txt").open("wb") as stderr:
            process = subprocess.Popen(command, stdout=stdout, stderr=stderr, cwd=cwd,
                                       env=env, start_new_session=True)
            try:
                wait_process(process, timeout)
            except subprocess.TimeoutExpired:
                record["status"] = "timeout"
            finally:
                collect_process_group(process)
            check_interruption()
            if record["status"] != "timeout":
                record["status"] = "passed" if process.returncode == 0 else "failed"
    except KeyboardInterrupt as error:
        record.update(status="interrupted", signal=getattr(error, "signum", signal.SIGINT))
        raise
    except OSError as error:
        record.update(status="unavailable", error=str(error), returncode=None)
    except BaseException as error:
        record.update(status="failed", error=f"{type(error).__name__}: {error}")
        raise
    finally:
        if process is not None:
            record["returncode"] = process.returncode
        record["wall_seconds"] = time.monotonic() - start
        write_json(output / "process.json", record)
    return record


def require_process(record):
    if record["status"] != "passed":
        raise ValueError(f"process {record['status']}: {record['command']}; exit={record['returncode']}")


def load_lock():
    return json.loads((HERE / "identity.json").read_text())


def verify_artifact(artifact, source, lock):
    tree = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD^{tree}"], text=True, timeout=10).strip()
    dirty = subprocess.check_output(["git", "-C", str(source), "status", "--porcelain", "--", "dal-cpp", "dal-python"], text=True, timeout=10)
    if tree != lock["published_tree"] or dirty:
        raise ValueError("source checkout is not the clean pinned product tree")
    for relative, digest in lock["artifact_files"].items():
        verify_hash(artifact / relative, digest)
    for relative, digest in lock["suite_files"].items():
        verify_hash(source / "dal-python/benchmarks" / relative, digest)
    for relative, digest in lock["headers"].items():
        verify_hash(source / relative, digest)
    result = json.loads((artifact / "python-paired/results.json").read_text())
    if (result["samples"], result["rounds"], result["threshold_percent"]) != (10, 2, 4.0):
        raise ValueError("original gate configuration mismatch")
    for side in ("base", "head"):
        if result["builds"][side]["sha"] != lock["source_shas"][side]:
            raise ValueError(f"original {side} source mismatch")


def prepare_packages(artifact, output, lock):
    packages = output / "packages"
    for side in ("base", "head"):
        destination = packages / side
        extract_package(artifact / f"python-reproduction/{side}-package.tar.gz", destination)
        binary = destination / "dal" / f"_dal.{ABI}.so"
        verify_hash(binary, lock["binaries"][side])
        shutil.copytree(destination, packages / f"{side}-copy")
        verify_hash(packages / f"{side}-copy/dal/{binary.name}", lock["binaries"][side])
    return packages


def preflight(packages, output):
    cpuinfo = Path("/proc/cpuinfo").read_text()
    (output / "cpuinfo.txt").write_text(cpuinfo)
    flag_sets = [set(line.split(":", 1)[1].split()) for line in cpuinfo.splitlines() if line.startswith("flags")]
    flags = set.intersection(*flag_sets) if flag_sets else set()
    state = {"schema": "dal201.preflight/1", "status": "running", "python": sys.version,
             "abi": sysconfig.get_config_var("SOABI"), "flags": sorted(flags), "platform": platform.platform(),
             "cpu_not_guaranteed_equal_to_original": True,
             "runner_image": {key: os.environ.get(key) for key in ("ImageOS", "ImageVersion")}}
    write_json(output / "preflight.json", state)
    try:
        require_runtime(flags, state["abi"])
        for side in ("base", "head"):
            package = packages / side
            command = [sys.executable, "-I", str(HERE / "worker.py"), "--package", str(package), "--import-only"]
            require_process(run_process(command, output / f"import-{side}", timeout=30, env=clean_environment()))
        state["status"] = "passed"
    except BaseException as error:
        state.update(status="failed", error=str(error))
        raise
    finally:
        write_json(output / "preflight.json", state)


def schedule():
    return [(index + 1, arm) for index in range(SAMPLES) for arm in (ARMS if index % 2 == 0 else ARMS[::-1])]


def worker_command(package, source, output, mode="plain"):
    return [sys.executable, "-I", str(HERE / "worker.py"), "--package", str(package),
            "--suite", str(source / "dal-python/benchmarks"), "--output", str(output), "--mode", mode]


def collect_controls(packages, source, output):
    for index, arm in schedule():
        destination = output / "controls" / f"{index:02d}-{arm}"
        command = worker_command(packages / arm, source, destination / "benchmark")
        require_process(run_process(command, destination, timeout=120, env=clean_environment(), cwd=output))
        verify_report(destination / "benchmark/results.json", arm)


def verify_report(path, arm):
    report = json.loads(path.read_text())
    lock = load_lock()
    if (report["status"] != "passed" or report["profile"] != "full"
            or report["samples"] != 1 or report["warmups"] != 2
            or report["environment"]["native_sha256"] != lock["binaries"][arm.removesuffix("-copy")]
            or report["environment"]["suite_sha256"] != lock["suite_files"]
            or [row["name"] for row in report["results"]] != lock["prefix"]):
        raise ValueError("worker identity/configuration/prefix failure")
    for row in report["results"]:
        if row["status"] != "passed" or len(row["samples_ns"]) != 1 or row["samples_ns"][0] <= 0:
            raise ValueError("worker validator/sample failure")
    return report


def summarize_controls(output):
    results = {}
    for target in TARGETS:
        raw = {}
        for arm in ARMS:
            raw[arm] = []
            for index in range(1, SAMPLES + 1):
                report = verify_report(output / "controls" / f"{index:02d}-{arm}/benchmark/results.json", arm)
                raw[arm].append(next(row["samples_ns"][0] for row in report["results"] if row["name"] == target))
        results[target] = {"samples_ns": raw, "round_minima_ns": {arm: [min(values[:10]), min(values[10:])] for arm, values in raw.items()}}
    write_json(output / "targets.json", {"schema": "dal201.targets/1", "acceptance_gate": False,
                                         "original_gate_status": "failed_87_of_90", "targets": results})


def manifest(output):
    files = {path.relative_to(output).as_posix(): sha256(path)
             for path in sorted(output.rglob("*")) if path.is_file() and path.name != "manifest.json"}
    write_json(output / "manifest.json", {"schema": "dal201.manifest/1", "files": files})


@cancellation_scope()
def main():
    global DEADLINE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--preflight-only", action="store_true")
    args = parser.parse_args()
    DEADLINE = time.monotonic() + 1800
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    summary = {"schema": "dal201.diagnostics/1", "status": "running", "acceptance_gate": False,
               "budget": {"control_processes": 80, "phase_processes": 4, "perf_processes": 2},
               "targets": list(TARGETS)}
    write_json(output / "summary.json", summary)
    try:
        lock = load_lock()
        verify_artifact(args.artifact.resolve(), args.source.resolve(), lock)
        write_json(output / "identity.json", lock)
        packages = prepare_packages(args.artifact.resolve(), output, lock)
        check_interruption()
        preflight(packages, output)
        if not args.preflight_only:
            from phases import collect_phases, collect_perf, prepare_phases
            prepared = prepare_phases(packages, args.source.resolve(), output)
            collect_controls(packages, args.source.resolve(), output)
            summarize_controls(output)
            collect_phases(packages, args.source.resolve(), output, prepared)
            collect_perf(packages, args.source.resolve(), output, prepared[2])
        check_interruption()
        summary["status"] = "preflight-passed" if args.preflight_only else "collected-with-root-attribution-gap"
        summary["root_phase"] = "unresolved: no verified standalone boundary in archived binaries"
        code = 0
    except KeyboardInterrupt as error:
        signum = getattr(error, "signum", signal.SIGINT)
        summary.update(status="interrupted", signal=signum, error=signal.Signals(signum).name)
        code = 128 + signum
    except BaseException as error:
        summary.update(status="failed", error=f"{type(error).__name__}: {error}")
        print(summary["error"], file=sys.stderr)
        code = 1
    finally:
        write_json(output / "summary.json", summary)
        manifest(output)
        if CANCEL_SIGNAL is not None and summary.get("signal") != CANCEL_SIGNAL:
            summary.update(status="interrupted", signal=CANCEL_SIGNAL, error=signal.Signals(CANCEL_SIGNAL).name)
            code = 128 + CANCEL_SIGNAL
            write_json(output / "summary.json", summary)
            manifest(output)
    return code


if __name__ == "__main__":
    sys.modules["diagnostics"] = sys.modules[__name__]
    raise SystemExit(main())

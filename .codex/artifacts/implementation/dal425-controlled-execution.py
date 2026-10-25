"""Linux-only finite Python worker candidate; all product execution is disabled."""
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import resource
import select
import signal
import subprocess
import sys
import time

AS_BYTES = 128 * 1024 * 1024
RECORD_BYTES = 8192
SETTLEMENT_BYTES = 16384
MARGIN = .25
ENV = {"LC_ALL": "C", "TZ": "UTC"}
ACTIONS = ("ok", "nonzero", "burn", "flood")
SOURCE = Path(__file__).resolve()


def identity():
    return {"source": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            "python": hashlib.sha256(Path(sys.executable).resolve().read_bytes()).hexdigest()}


def _positive(value):
    return type(value) in (float, int) and math.isfinite(value) and value > 0


def _append(fd, record, cap):
    data = (json.dumps(record, sort_keys=True, allow_nan=False) + "\n").encode()
    if len(data) > RECORD_BYTES or os.fstat(fd).st_size + len(data) > cap:
        raise ValueError("ledger byte budget")
    os.lseek(fd, 0, os.SEEK_END)
    if os.write(fd, data) != len(data):
        raise OSError("partial ledger write; UNKNOWN")
    os.fsync(fd)


def _load(fd):
    size = os.fstat(fd).st_size
    if size > 1048576:
        raise ValueError("ledger size")
    os.lseek(fd, 0, os.SEEK_SET)
    data = os.read(fd, size + 1)
    try:
        if not data.endswith(b"\n"):
            raise ValueError("torn ledger")
        rows = [json.loads(line) for line in data.splitlines()]
        allocation = rows[0]
        valid = (allocation["kind"] == "allocation" and allocation["domain"] == "synthetic"
                 and allocation["product_actual"] == 0
                 and all(_positive(allocation[k]) for k in ("wall", "cpu", "bytes", "units"))
                 and type(allocation["bytes"]) is int and allocation["bytes"] <= 1048576)
        if not valid or size > allocation["bytes"]:
            raise ValueError("invalid allocation")
        used = {"wall": 0., "cpu": 0., "units": 0}
        pending = None
        attempts = 0
        for row in rows[1:]:
            if row["kind"] == "reserve" and pending is None and row["id"] == attempts + 1:
                for k in used:
                    if not _positive(row[k]):
                        raise ValueError("bad reservation")
                    used[k] += row[k]
                attempts += 1
                pending = row["id"]
            elif row["kind"] == "settle" and row["id"] == pending and pending is not None:
                if row["status"] != "complete":
                    raise ValueError("partial ledger; UNKNOWN successor")
                pending = None
            else:
                raise ValueError("invalid ledger transition")
        if pending is not None:
            raise ValueError("UNKNOWN pending reservation")
        if any(used[k] > allocation[k] for k in used):
            raise ValueError("overdrawn ledger")
        return allocation, used, attempts
    except (KeyError, IndexError, TypeError, json.JSONDecodeError, ValueError) as error:
        raise ValueError("ledger: " + str(error)) from error


def _stat(pid):
    fields = Path("/proc", str(pid), "stat").read_text().rsplit(")", 1)[1].split()
    return fields[19], (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def _child_cpu(pid):
    return _stat(pid)[1]


def _limits(cpu):
    resource.setrlimit(resource.RLIMIT_AS, (AS_BYTES, AS_BYTES))
    resource.setrlimit(resource.RLIMIT_CPU, (math.ceil(cpu + MARGIN), math.ceil(cpu + MARGIN)))
    resource.setrlimit(resource.RLIMIT_FSIZE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def _drain(fd, captured, cap):
    while True:
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            return False
        if not chunk:
            return False
        if len(captured) + len(chunk) > cap:
            captured.extend(chunk[:max(0, cap - len(captured))])
            return True
        captured.extend(chunk)


def _stop(pid, starttime, pidfd):
    if _stat(pid)[0] != starttime:
        raise RuntimeError("UNKNOWN child identity changed")
    if pidfd is not None:
        signal.pidfd_send_signal(pidfd, signal.SIGKILL)
    else:
        # An unreaped direct child retains its PID, including after exit.
        os.kill(pid, signal.SIGKILL)


def _execute(root, action, wall, cpu, output_bytes, started, own_started):
    argv = [str(Path(sys.executable).resolve()), "-I", "-S", "-B", str(SOURCE), "--worker", action]
    proc = subprocess.Popen(argv, cwd=root, env=ENV, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            preexec_fn=lambda: _limits(cpu), close_fds=True)
    pid = proc.pid
    pidfd = None
    captured = bytearray()
    reason = "exit"
    status = None
    usage = None
    try:
        starttime = _stat(pid)[0]
        if hasattr(os, "pidfd_open") and hasattr(signal, "pidfd_send_signal"):
            pidfd = os.pidfd_open(pid)
        os.set_blocking(proc.stdout.fileno(), False)
        while True:
            got, status, usage = os.wait4(pid, os.WNOHANG)
            if got:
                if _drain(proc.stdout.fileno(), captured, output_bytes):
                    reason = "output"
                break
            if _drain(proc.stdout.fileno(), captured, output_bytes):
                reason = "output"
                break
            if time.monotonic() - started >= wall:
                reason = "wall"
                break
            if _child_cpu(pid) + time.process_time() - own_started >= cpu:
                reason = "cpu"
                break
            select.select([proc.stdout], [], [], .001)
    except BaseException as error:
        reason = "UNKNOWN " + type(error).__name__
    finally:
        if usage is None:
            # wait4 has not reaped this direct child, so its PID is still owned.
            _stop(pid, starttime, pidfd)
            _, status, usage = os.wait4(pid, 0)
        proc.returncode = os.waitstatus_to_exitcode(status)
        proc.stdout.close()
        if pidfd is not None:
            os.close(pidfd)
    return {"kind": "settle", "status": "complete" if reason == "exit" and proc.returncode == 0 else "partial",
            "reason": reason, "pid": pid, "starttime": starttime, "argv": argv,
            "exit": proc.returncode, "wait_status": status, "child_cpu": usage.ru_utime + usage.ru_stime,
            "supervisor_cpu": time.process_time() - own_started, "wall": time.monotonic() - started,
            "maxrss_kib": usage.ru_maxrss, "stdout": captured.decode("ascii", errors="replace"),
            "product_actual": 0}


def run(root, identity, action="ok", *, wall=.4, cpu=.2, output_bytes=128, units=1, products=0):
    started, own_started = time.monotonic(), time.process_time()
    if not identity or identity != globals()["identity"]():
        raise ValueError("identity missing or mismatched")
    if action not in ACTIONS:
        raise ValueError("action disabled")
    if products != 0:
        raise ValueError("product execution disabled")
    if not (_positive(wall) and wall <= 2 and _positive(cpu) and cpu <= 2
            and type(output_bytes) is int and 0 <= output_bytes <= 4096
            and type(units) is int and 0 < units <= 100):
        raise ValueError("invalid budget")
    resource.setrlimit(resource.RLIMIT_AS, (AS_BYTES, AS_BYTES))
    root = Path(root).resolve(strict=True)
    fd = os.open(root / "ledger.jsonl", os.O_RDWR | os.O_NOFOLLOW)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX)
        allocation, used, attempts = _load(fd)
        reservation = {"kind": "reserve", "id": attempts + 1, "wall": wall + MARGIN,
                       "cpu": cpu + MARGIN, "units": units, "identity": identity,
                       "cwd": str(root), "env": ENV, "action": action, "output_bytes": output_bytes}
        if any(used[k] + reservation[k] > allocation[k] for k in used):
            raise ValueError("ledger budget exhausted")
        if os.fstat(fd).st_size + RECORD_BYTES + SETTLEMENT_BYTES > allocation["bytes"]:
            raise ValueError("ledger byte budget: settlement reserve")
        _append(fd, reservation, allocation["bytes"])
        result = _execute(root, action, wall, cpu, output_bytes, started, own_started)
        result["id"] = reservation["id"]
        if (result["wall"] > reservation["wall"] or
                result["child_cpu"] + result["supervisor_cpu"] > reservation["cpu"]):
            result.update(status="partial", reason="UNKNOWN reservation exceeded")
        _append(fd, result, allocation["bytes"])
        return result
    finally:
        os.close(fd)


def _worker(action):
    if action == "nonzero":
        return 7
    if action == "burn":
        while True:
            pass
    os.write(1, b"x" * 4096 if action == "flood" else b"ok\n")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--worker" and sys.argv[2] in ACTIONS:
        raise SystemExit(_worker(sys.argv[2]))
    raise SystemExit("Private API only: synthetic ledger required; product/compiler modes disabled")

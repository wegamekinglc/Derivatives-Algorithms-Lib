"""Linux-only finite Python worker candidate; all product execution is disabled."""
import fcntl
import hashlib
import json
import math
import marshal
import os
from pathlib import Path
import resource
import select
import signal
import subprocess
import stat
import sys
import time

AS_BYTES = 128 * 1024 * 1024
RECORD_BYTES = 8192
SETTLEMENT_BYTES = 16384
MARGIN = .25
_CLEANUP_CLOCK = time.monotonic
ENV = {"LC_ALL": "C", "TZ": "UTC"}
ACTIONS = ("ok", "nonzero", "burn", "flood")
SOURCE = Path(__file__).absolute()
SOURCE_BYTES = globals().get("_SOURCE_BYTES") or SOURCE.read_bytes()
FRESH_ENTRY = globals().get("_FRESH_ENTRY", False)


def _runtime():
    files = {}
    for module in tuple(sys.modules.values()):
        name = getattr(module, "__file__", None)
        if name and name != str(SOURCE) and not name.startswith("/proc/self/fd/"):
            path = Path(name).resolve()
            if path.is_file():
                files[str(path)] = None
    for line in Path("/proc/self/maps").read_text().splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) == 6 and fields[5].startswith("/"):
            path = Path(fields[5])
            info = path.stat()
            device = "%02x:%02x" % (os.major(info.st_dev), os.minor(info.st_dev))
            if str(info.st_ino) != fields[4] or device != fields[3]:
                raise ValueError("mapped runtime identity changed")
            files[str(path)] = None
    total = 0
    for name in files:
        total += Path(name).stat().st_size
        if total > 64 * 1024 * 1024:
            raise ValueError("runtime identity byte cap")
        data = Path(name).read_bytes()
        files[name] = hashlib.sha256(data).hexdigest()
    return {"python": hashlib.sha256(Path("/proc/self/exe").read_bytes()).hexdigest(),
            "version": sys.version, "files": files}


def identity():
    return json.loads(json.dumps(_IDENTITY))


def _entry_check():
    if not FRESH_ENTRY:
        raise ValueError("fixed fresh entry required")
    pages = int(Path("/proc/self/statm").read_text().split()[0])
    if (pages * os.sysconf("SC_PAGE_SIZE") > AS_BYTES
            or not 0 < resource.getrlimit(resource.RLIMIT_AS)[1] <= AS_BYTES
            or not 0 < resource.getrlimit(resource.RLIMIT_CPU)[1] <= 3
            or "Threads:\t1\n" not in Path("/proc/self/status").read_text()
            or Path("/proc/self/task", str(os.getpid()), "children").read_text().strip()):
        raise ValueError("fixed entry limits/mapping/process boundary")
    if hashlib.sha256(json.dumps(_runtime(), sort_keys=True).encode()).hexdigest() != _IDENTITY["runtime"]:
        raise ValueError("loaded runtime changed")


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


def _number(value, *, integer=False):
    return type(value) in ((int,) if integer else (int, float)) and math.isfinite(value) and value >= 0


def _keys(row, required):
    if type(row) is not dict or set(row) != set(required.split()):
        raise ValueError("record fields")


def _binding(row, allocation):
    for key in ("identity", "cwd", "env", "action"):
        if row[key] != allocation[key]:
            raise ValueError("ledger identity binding")


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def _load(fd):
    size = os.fstat(fd).st_size
    if size > 1048576:
        raise ValueError("ledger size")
    os.lseek(fd, 0, os.SEEK_SET)
    data = os.read(fd, size + 1)
    try:
        if not data.endswith(b"\n") or len(data) != size:
            raise ValueError("torn ledger")
        rows = [json.loads(line, object_pairs_hook=_pairs) for line in data.splitlines()]
        allocation = rows[0]
        _keys(allocation, "kind schema domain product_actual wall cpu bytes units original_allocation identity cwd env action")
        _keys(allocation["identity"], "source loaded_code runtime")
        if not all(type(value) is str and len(value) == 64 and all(c in "0123456789abcdef" for c in value)
                   for value in allocation["identity"].values()):
            raise ValueError("allocation identity digest")
        if (allocation["kind"] != "allocation" or type(allocation["schema"]) is not int or allocation["schema"] != 2
                or allocation["domain"] != "synthetic" or type(allocation["product_actual"]) is not int
                or allocation["product_actual"] != 0 or allocation["original_allocation"] != "UNKNOWN"
                or not all(_positive(allocation[k]) for k in ("wall", "cpu", "bytes", "units"))
                or type(allocation["bytes"]) is not int or allocation["bytes"] > 1048576
                or type(allocation["units"]) is not int or size > allocation["bytes"]
                or type(allocation["identity"]) is not dict or not allocation["identity"]
                or allocation["env"] != ENV or type(allocation["cwd"]) is not str or allocation["action"] not in ACTIONS):
            raise ValueError("invalid allocation")
        used = {"wall": 0., "cpu": 0., "units": 0}
        pending, attempts = None, 0
        for row in rows[1:]:
            if row.get("kind") == "reserve" and pending is None:
                _keys(row, "kind id wall cpu units identity cwd env action output_bytes")
                if type(row["id"]) is not int or row["id"] != attempts + 1:
                    raise ValueError("reservation order")
                _binding(row, allocation)
                if (row["action"] not in ACTIONS or type(row["output_bytes"]) is not int
                        or not 0 <= row["output_bytes"] <= 4096 or type(row["units"]) is not int):
                    raise ValueError("reservation parameters")
                for key in used:
                    if not _positive(row[key]):
                        raise ValueError("bad reservation")
                    used[key] += row[key]
                attempts += 1
                pending = row
            elif row.get("kind") == "settle" and pending is not None:
                _keys(row, "kind id status reason errors pid starttime argv reaped exit wait_status child_cpu supervisor_cpu wall maxrss_kib stdout product_actual identity cwd env action")
                _binding(row, allocation)
                if type(row["id"]) is not int or row["id"] != pending["id"] or row["action"] != pending["action"]:
                    raise ValueError("settlement binding")
                if row["status"] != "complete":
                    raise ValueError("partial ledger; UNKNOWN successor")
                if (row["reaped"] is not True or row["exit"] != 0 or type(row["exit"]) is not int
                        or type(row["wait_status"]) is not int or row["wait_status"] != 0
                        or type(row["pid"]) is not int or row["pid"] <= 0
                        or type(row["starttime"]) is not str or not row["starttime"].isdecimal()
                        or row["reason"] != "exit" or row["errors"] != []
                        or type(row["product_actual"]) is not int or row["product_actual"] != 0
                        or not all(_number(row[k]) for k in ("child_cpu", "supervisor_cpu", "wall"))
                        or not _number(row["maxrss_kib"], integer=True) or not 0 < row["maxrss_kib"] <= AS_BYTES // 1024
                        or row["wall"] > pending["wall"]
                        or row["child_cpu"] + row["supervisor_cpu"] > pending["cpu"]
                        or type(row["stdout"]) is not str or len(row["stdout"].encode()) > pending["output_bytes"]
                        or not _valid_argv(row["argv"], pending["action"])):
                    raise ValueError("invalid actual wait4 receipt")
                pending = None
            else:
                raise ValueError("invalid ledger transition")
        if pending is not None:
            raise ValueError("UNKNOWN pending reservation")
        if any(used[k] > allocation[k] for k in used):
            raise ValueError("overdrawn ledger")
        return allocation, used, attempts
    except (KeyError, IndexError, TypeError, AttributeError, json.JSONDecodeError, ValueError) as error:
        raise ValueError("ledger: " + str(error)) from error


def _argv(action, fd=3):
    return [str(Path(sys.executable).resolve()), "-I", "-S", "-B", "/proc/self/fd/" + str(fd), "--worker", action]


def _valid_argv(argv, action):
    if type(argv) is not list or len(argv) != 7 or type(argv[4]) is not str:
        return False
    prefix = "/proc/self/fd/"
    suffix = argv[4].removeprefix(prefix)
    return suffix.isdecimal() and int(suffix) >= 3 and argv == _argv(action, int(suffix))


def _snapshot():
    fd = os.memfd_create("dal425-fixed-source", os.MFD_ALLOW_SEALING | os.MFD_CLOEXEC)
    try:
        if os.write(fd, SOURCE_BYTES) != len(SOURCE_BYTES):
            raise OSError("source snapshot short write")
        os.lseek(fd, 0, os.SEEK_SET)
        fcntl.fcntl(fd, fcntl.F_ADD_SEALS, fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL)
        return fd
    except BaseException:
        os.close(fd)
        raise


def _checkpoint(started, own_started, wall, cpu):
    if time.monotonic() - started >= wall:
        raise ValueError("wall budget exhausted before spawn")
    if time.process_time() - own_started >= cpu:
        raise ValueError("CPU budget exhausted before spawn")


def _lock(fd, started, own_started, wall, cpu):
    while True:
        _checkpoint(started, own_started, wall, cpu)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            return
        except (BlockingIOError, InterruptedError):
            select.select([], [], [], .001)


def _open_ledger(root):
    before = (root / "ledger.jsonl").lstat()
    if not stat.S_ISREG(before.st_mode):
        raise ValueError("ledger must be a regular file")
    fd = os.open(root / "ledger.jsonl", os.O_RDWR | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
    current = os.fstat(fd)
    if (not stat.S_ISREG(current.st_mode) or current.st_uid != os.getuid()
            or current.st_mode & 0o077 or current.st_nlink != 1
            or (before.st_dev, before.st_ino) != (current.st_dev, current.st_ino)):
        os.close(fd)
        raise ValueError("ledger private file identity")
    return fd


def _guard(root):
    # Permanent in this candidate: there is no automatic restart/ack protocol.
    fd = os.open(root / "pending", os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    try:
        data = b"UNKNOWN until independent durable acknowledgement; do not clear\n"
        if os.write(fd, data) != len(data):
            raise OSError("guard short write")
        os.fsync(fd)
    finally:
        os.close(fd)
    directory = os.open(root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


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
    # The caller alone owns this unreaped, direct child; no other reaper runs.
    if pidfd is not None:
        signal.pidfd_send_signal(pidfd, signal.SIGKILL)
        return
    if starttime is not None:
        try:
            observed = _stat(pid)[0]
        except FileNotFoundError:
            observed = starttime
        if observed != starttime:
            raise RuntimeError("UNKNOWN child identity changed")
    os.kill(pid, signal.SIGKILL)


def _execute(root, action, wall, cpu, output_bytes, started, own_started):
    _checkpoint(started, own_started, wall, cpu)
    snapshot = _snapshot()
    argv = _argv(action, snapshot)
    try:
        proc = subprocess.Popen(argv, cwd=root, env=ENV, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                preexec_fn=lambda: _limits(cpu), close_fds=True, pass_fds=(snapshot,))
    except BaseException as error:
        try:
            os.close(snapshot)
        except BaseException as close_error:
            error.add_note("snapshot close: " + str(close_error))
        raise
    pid, pidfd, starttime = proc.pid, None, None
    captured, errors = bytearray(), []
    reason, receipt = "exit", None

    def collect():
        nonlocal receipt
        try:
            got, status, usage = os.wait4(pid, os.WNOHANG)
        except InterruptedError:
            return
        if got == pid:
            if receipt is not None:
                raise RuntimeError("duplicate wait4")
            receipt = (status, usage)
        elif got != 0:
            raise RuntimeError("unexpected wait4 PID")

    def preserve(error):
        if len(errors) < 16:
            errors.append(type(error).__name__ + ": " + str(error)[:256])

    try:
        if hasattr(os, "pidfd_open") and hasattr(signal, "pidfd_send_signal"):
            try:
                pidfd = os.pidfd_open(pid)
            except OSError:
                pass
        starttime = _stat(pid)[0]
        os.set_blocking(proc.stdout.fileno(), False)
        while True:
            collect()
            if _drain(proc.stdout.fileno(), captured, output_bytes):
                reason = "output"
                break
            if receipt is not None:
                break
            if time.monotonic() - started >= wall:
                reason = "wall"
                break
            if _child_cpu(pid) + time.process_time() - own_started >= cpu:
                reason = "cpu"
                break
            select.select([proc.stdout], [], [], .001)
    except BaseException as error:
        preserve(error)
    finally:
        deadline = _CLEANUP_CLOCK() + MARGIN
        if receipt is None:
            for _ in range(3):
                try:
                    _stop(pid, starttime, pidfd)
                    break
                except InterruptedError as error:
                    preserve(error)
                except ProcessLookupError:
                    break
                except BaseException as error:
                    preserve(error)
                    break
            while receipt is None and _CLEANUP_CLOCK() < deadline:
                try:
                    collect()
                except BaseException as error:
                    preserve(error)
                    if not isinstance(error, InterruptedError):
                        break
                if receipt is None:
                    try:
                        select.select([], [], [], .001)
                    except BaseException as error:
                        preserve(error)
        # Each close is attempted even if another cleanup operation failed.
        for close in (proc.stdout.close, lambda: os.close(snapshot), (lambda: os.close(pidfd)) if pidfd is not None else (lambda: None)):
            try:
                close()
            except BaseException as error:
                preserve(error)
        if receipt is not None:
            proc.returncode = os.waitstatus_to_exitcode(receipt[0])
    status, usage = receipt if receipt is not None else (None, None)
    return {"kind": "settle", "status": "complete" if receipt is not None and not errors and reason == "exit" and proc.returncode == 0 else "partial",
            "reason": "UNKNOWN" if errors or receipt is None else reason, "errors": errors,
            "pid": pid, "starttime": starttime, "argv": argv, "reaped": receipt is not None,
            "exit": proc.returncode if receipt is not None else None, "wait_status": status,
            "child_cpu": usage.ru_utime + usage.ru_stime if usage is not None else None,
            "supervisor_cpu": time.process_time() - own_started, "wall": time.monotonic() - started,
            "maxrss_kib": usage.ru_maxrss if usage is not None else None,
            "stdout": captured.decode("ascii", errors="replace"), "product_actual": 0}


def run(root, identity, action="ok", *, wall=.4, cpu=.2, output_bytes=128, units=1, products=0):
    started, own_started = time.monotonic(), time.process_time()
    if not identity or identity != globals()["identity"]():
        raise ValueError("identity missing or mismatched")
    if action not in ACTIONS:
        raise ValueError("action disabled")
    if type(products) is not int or products != 0:
        raise ValueError("product execution disabled")
    if not (_positive(wall) and wall <= 2 and _positive(cpu) and cpu <= 2
            and type(output_bytes) is int and 0 <= output_bytes <= 4096
            and type(units) is int and 0 < units <= 100):
        raise ValueError("invalid budget")
    _entry_check()
    _checkpoint(started, own_started, wall, cpu)
    root = Path(root).absolute()
    info = root.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
        raise ValueError("private fixed directory required")
    fd = _open_ledger(root)
    try:
        _lock(fd, started, own_started, wall, cpu)
        if (root / "pending").exists():
            raise ValueError("UNKNOWN pending guard; no successor")
        allocation, used, attempts = _load(fd)
        _checkpoint(started, own_started, wall, cpu)
        if allocation["identity"] != identity or allocation["cwd"] != str(root) or allocation["action"] != action:
            raise ValueError("ledger identity binding")
        if attempts:
            raise ValueError("UNKNOWN prior attempt; no automatic restart")
        reservation = {"kind": "reserve", "id": attempts + 1, "wall": wall + MARGIN,
                       "cpu": cpu + MARGIN, "units": units, "identity": identity,
                       "cwd": str(root), "env": ENV, "action": action, "output_bytes": output_bytes}
        if any(used[k] + reservation[k] > allocation[k] for k in used):
            raise ValueError("ledger budget exhausted")
        if os.fstat(fd).st_size + RECORD_BYTES + SETTLEMENT_BYTES > allocation["bytes"]:
            raise ValueError("ledger byte budget: settlement reserve")
        _checkpoint(started, own_started, wall, cpu)
        _guard(root)
        _append(fd, reservation, allocation["bytes"])
        _checkpoint(started, own_started, wall, cpu)
        result = _execute(root, action, wall, cpu, output_bytes, started, own_started)
        result.update(id=reservation["id"], identity=identity, cwd=str(root), env=ENV, action=action)
        if (result["wall"] > reservation["wall"] or
                not result.get("reaped", False) or result["child_cpu"] + result["supervisor_cpu"] > reservation["cpu"]):
            result.update(status="partial", reason="UNKNOWN reservation exceeded")
        _append(fd, result, allocation["bytes"])
        return result
    finally:
        os.close(fd)


def _worker(action):
    if action == "nonzero":
        return 7
    if action == "burn":
        os.write(1, b"ready\n")
        while True:
            pass
    os.write(1, b"x" * 4096 if action == "flood" else b"ok\n")
    return 0


def _command(mode, root, control):
    root = Path(root).absolute()
    os.chdir(root)
    fixed = {"ok": ("ok", .5, .3, 128), "nonzero": ("nonzero", .5, .3, 128),
             "burn": ("burn", 1., .12, 128), "deadline": ("burn", .12, .6, 128),
             "flood": ("flood", .5, .3, 1)}
    if control not in fixed:
        raise ValueError("control disabled")
    _entry_check()
    if mode == "init":
        info = root.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
            raise ValueError("private directory required")
        fd = os.open(root / "ledger.jsonl", os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            _append(fd, {"kind":"allocation", "schema":2, "domain":"synthetic", "product_actual":0,
                         "wall":10., "cpu":10., "bytes":1048576, "units":100, "original_allocation":"UNKNOWN",
                         "identity":identity(), "cwd":str(root), "env":ENV, "action":fixed[control][0]}, 1048576)
        finally:
            os.close(fd)
        print(json.dumps({"initialized":True, "identity":identity(), "runtime_manifest":_runtime()}, sort_keys=True))
        return
    if mode != "run":
        raise ValueError("mode disabled")
    action, wall, cpu, output = fixed[control]
    def exhausted(signum, frame):
        raise ValueError("supervisor CPU timer exhausted")
    signal.signal(signal.SIGPROF, exhausted)
    signal.setitimer(signal.ITIMER_PROF, cpu)
    try:
        result = run(root, identity(), action, wall=wall, cpu=cpu, output_bytes=output)
    finally:
        signal.setitimer(signal.ITIMER_PROF, 0)
    print(json.dumps(result, sort_keys=True, allow_nan=False))


_IS_WORKER = len(sys.argv) == 3 and sys.argv[1] == "--worker" and sys.argv[2] in ACTIONS
_IDENTITY = None if _IS_WORKER else {"source":hashlib.sha256(SOURCE_BYTES).hexdigest(),
             "loaded_code":hashlib.sha256(b"".join(marshal.dumps(value.__code__)
                             for key, value in sorted(globals().items()) if hasattr(value, "__code__"))).hexdigest(),
             "runtime":hashlib.sha256(json.dumps(_runtime(), sort_keys=True).encode()).hexdigest()}

if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--worker" and sys.argv[2] in ACTIONS:
        raise SystemExit(_worker(sys.argv[2]))
    raise SystemExit("Use the fixed private launcher; product/compiler modes disabled")

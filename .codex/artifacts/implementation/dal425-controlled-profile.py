"""Linux fixed-host, worker-free profile/init/verify one-shot candidate."""
import sys

BOOT_CODE = sys._getframe().f_code
EXECUTED = []
SOURCES = {}
INITIAL = tuple(sys.modules)


def _audit(event, args):
    if event == "exec":
        EXECUTED.append(args[0])


if __name__ == "__main__":
    import os
    import resource
    resource.setrlimit(resource.RLIMIT_AS, (128 * 1024 * 1024,) * 2)
    resource.setrlimit(resource.RLIMIT_CPU, (1, 1) if sys.argv[1:2] == ["--child"] else (2, 2))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_FSIZE, (1048576,) * 2)
    resource.setrlimit(resource.RLIMIT_NOFILE, (32,) * 2)
    INITIAL = tuple(sys.modules)
    sys.addaudithook(_audit)
    import _frozen_importlib_external as _external

    class SourceLoader:
        def __init__(self, path):
            self.path = path

        def create_module(self, spec):
            return None

        def exec_module(self, module):
            with open(self.path, "rb") as stream:
                source = stream.read(1048577)
            if len(source) > 1048576:
                raise ValueError("source cap")
            code = compile(source, self.path, "exec", dont_inherit=True, optimize=sys.flags.optimize)
            SOURCES[self.path] = (source, code)
            exec(code, module.__dict__)

    class SourceFinder:
        def find_spec(self, fullname, path=None, target=None):
            spec = _external.PathFinder.find_spec(fullname, path, target)
            if spec and spec.origin and spec.origin.endswith(".py"):
                spec.loader = SourceLoader(spec.origin)
            elif spec and spec.origin and spec.origin.endswith(".pyc"):
                raise ValueError("standalone bytecode disabled")
            return spec

    sys.meta_path[sys.meta_path.index(_external.PathFinder)] = SourceFinder()

import fcntl
import hashlib
import io
import json
import marshal
import os
from pathlib import Path
import resource
import select
import signal
import stat
import subprocess
import time

ENV = {"LC_ALL": "C", "TZ": "UTC"}
# Each slot is preheld once, including unused/error slots; deletion never refunds.
OUTPUTS = {"outer.pending": 1024, "outer.reserve.json": 8192, "outer.receipt.json": 131072,
           "partial.json": 8192, "stdout.log": 131072, "profile.json": 131072,
           "ledger.jsonl": 8192, "manifest.json": 16384, "evidence.zip": 262144,
           "source.py": 65536, "fixture.json": 16384, "meter.json": 16384, "launch.ticket": 1024}
WALL = 4
CPU = 6


def verify_loaded_code(code, source, filename):
    expected = compile(source, filename, "exec", dont_inherit=True, optimize=sys.flags.optimize)
    if code != expected or code.co_filename != filename:
        raise ValueError("loaded code/source mismatch")


def _code_data(code):
    def constant(value):
        if isinstance(value, type(code)):
            return _code_data(value)
        if isinstance(value, (tuple, frozenset)):
            values = [constant(item) for item in value]
            return [type(value).__name__, sorted(values, key=repr) if isinstance(value, frozenset) else values]
        return [type(value).__name__, repr(value)]
    return [code.co_code.hex(), [constant(v) for v in code.co_consts], code.co_names,
            code.co_varnames, code.co_freevars, code.co_cellvars, code.co_flags,
            code.co_argcount, code.co_posonlyargcount, code.co_kwonlyargcount,
            code.co_name, code.co_qualname, code.co_filename, code.co_firstlineno,
            code.co_linetable.hex(), code.co_exceptiontable.hex()]


def _json(value):
    return json.dumps(value, sort_keys=True, allow_nan=False).encode()


def _digest(data):
    return hashlib.sha256(data).hexdigest()


def _read(path, cap=1048576):
    with open(path, "rb") as stream:
        data = stream.read(cap + 1)
    if len(data) > cap:
        raise ValueError("input cap")
    return data


def _nested(code):
    result = {(code.co_qualname, code.co_firstlineno): code}
    for value in code.co_consts:
        if isinstance(value, type(code)):
            result.update(_nested(value))
    return result


def startup_profile():
    path = str(Path(__file__).resolve())
    data = _read(path)
    verify_loaded_code(BOOT_CODE, data, BOOT_CODE.co_filename)
    initial = {}
    for name in INITIAL:
        module = sys.modules[name]
        spec = getattr(module, "__spec__", None)
        origin = getattr(spec, "origin", None)
        if origin in (None, "built-in", "frozen"):
            initial[name] = {"origin": origin}
            continue
        if origin.endswith(".so"):
            initial[name] = {"origin": origin, "sha256": _digest(_read(origin, 16777216))}
            continue
        if not origin.endswith(".py"):
            raise ValueError("unbound initial module: " + name)
        source = _read(origin)
        expected = compile(source, origin, "exec", dont_inherit=True, optimize=sys.flags.optimize)
        cache = getattr(module, "__cached__", None)
        cache_hash = None
        if cache and Path(cache).exists():
            cache_bytes = _read(cache)
            verify_loaded_code(marshal.loads(cache_bytes[16:]), source, origin)
            cache_hash = _digest(cache_bytes)
        nested = _nested(expected)
        for value in vars(module).values():
            if getattr(value, "__module__", None) != name:
                continue
            code = getattr(value, "__code__", None)
            if code is not None and nested.get((code.co_qualname, code.co_firstlineno)) != code:
                raise ValueError("loaded initial function/source mismatch: " + name)
        initial[name] = {"origin": origin, "source": _digest(source), "cache": cache_hash,
                         "selection": "source/cache code-equivalent; faithful fixed CPython loader assumed"}
    if len(SOURCES) > 128 or sum(len(value[0]) for value in SOURCES.values()) > 8388608 or len(EXECUTED) > 256:
        raise ValueError("fixed dependency cap")
    sources = {}
    for name, (source, code) in tuple(SOURCES.items()):
        verify_loaded_code(code, source, name)
        if _read(name) != source:
            raise ValueError("source changed after load")
        sources[name] = {"source": _digest(source), "executed_code": _digest(_json(_code_data(code))),
                         "selection": "forced source, cache bypassed"}
    mapped = {}
    for line in Path("/proc/self/maps").read_text().splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) == 6 and fields[5].startswith("/"):
            file = Path(fields[5]); info = file.stat()
            if str(info.st_ino) != fields[4] or "%02x:%02x" % (os.major(info.st_dev), os.minor(info.st_dev)) != fields[3]:
                raise ValueError("mapped identity changed")
            mapped[str(file)] = _digest(_read(file, 33554432))
    if len(mapped) > 64 or sum(Path(name).stat().st_size for name in mapped) > 67108864:
        raise ValueError("mapped dependency cap")
    return {"schema": 1, "python": sys.version, "executable": str(Path(sys.executable).resolve()),
            "bootstrap": {"source": _digest(data), "executed_code": _digest(_json(_code_data(BOOT_CODE)))},
            "initial": initial, "sources": sources, "mapped": mapped,
            "executions": [_digest(_json(_code_data(code))) for code in tuple(EXECUTED)],
            "env": ENV, "cwd": str(Path.cwd()), "worker_enabled": False}


class OneShot:
    def __init__(self, root, budget):
        self.root = Path(root)
        self.budget = budget
        self.written = set()
        self.active = False

    def reserve(self):
        info = self.root.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
            raise ValueError("private root required")
        if (self.root / "outer.pending").exists():
            raise ValueError("pending: no successor or restart")
        required = {"wall": WALL, "cpu": CPU, "bytes": sum(OUTPUTS.values()), "files": len(OUTPUTS)}
        if (type(self.budget) is not dict or set(self.budget) != set(required)
                or any(type(self.budget[k]) is not int or self.budget[k] < v for k, v in required.items())):
            raise ValueError("outer budget exhausted or invalid")
        self.active = True
        self.write("outer.pending", _json({"held": required, "status": "permanent pending; no refund"}))
        self.write("outer.reserve.json", _json({"held": required, "slots": OUTPUTS, "budget": self.budget}))

    def write(self, name, data):
        if not self.active or name not in OUTPUTS or name in self.written or len(data) > OUTPUTS[name]:
            raise ValueError("output predebit/slot exhausted")
        self.written.add(name)
        fd = os.open(self.root / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            if os.write(fd, data) != len(data):
                raise OSError("short write; pending remains")
            os.fsync(fd)
        finally:
            os.close(fd)
        directory = os.open(self.root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)


def _tool():
    path = Path(__file__).with_name("dal425-controlled-execution.py").resolve()
    source = _read(path, 65536)
    code = compile(source, str(path), "exec", dont_inherit=True)
    SOURCES[str(path)] = (source, code)
    namespace = {"__name__": "dal425_profile_init", "__file__": str(path),
                 "_SOURCE_BYTES": source, "_FRESH_ENTRY": True}
    exec(code, namespace)
    return namespace


def _child(mode, root):
    tool = _tool()
    profile = startup_profile()
    if mode in ("verify", "init"):
        expected = json.loads(_read(root / "expected-profile.json", 131072))
        if expected != profile:
            raise ValueError("fixed startup profile mismatch")
    output = {"profile": profile, "mode": mode, "products": 0, "workers": 0}
    if mode == "init":
        capture = io.StringIO()
        old = sys.stdout; sys.stdout = capture
        try:
            tool["_command"]("init", str(root), "ok")
        finally:
            sys.stdout = old
        output["init"] = json.loads(capture.getvalue())
    data = _json(output)
    if len(data) > OUTPUTS["stdout.log"] or os.write(1, data) != len(data):
        raise ValueError("fixed output cap/short pipe write")


def _outer(mode, root):
    if dict(os.environ) != ENV:
        raise ValueError("fixed environment required")
    observer_profile = startup_profile()
    if mode in ("verify", "init") and json.loads(_read(root / "expected-observer.json", 131072)) != observer_profile:
        raise ValueError("fixed observer profile mismatch")
    budget = json.loads(_read(root / "budget.json", 1024))
    attempt = OneShot(root, budget)
    attempt.reserve()
    started = int(Path("/proc/self/stat").read_text().rsplit(")", 1)[1].split()[19]) / os.sysconf("SC_CLK_TCK")
    if time.monotonic() - started > .5:
        raise ValueError("startup wall allowance exhausted")
    if int(Path("/proc/self/statm").read_text().split()[0]) * os.sysconf("SC_PAGE_SIZE") > 128 * 1024 * 1024:
        raise ValueError("initial mapping exceeds AS")
    ticket = os.memfd_create("dal425-outer-ticket", os.MFD_ALLOW_SEALING | os.MFD_CLOEXEC)
    token = _json({"parent": os.getpid(), "mode": mode, "root": str(root),
                   "source": _digest(_read(Path(__file__).resolve()))})
    attempt.write("launch.ticket", token)
    if os.write(ticket, token) != len(token):
        os.close(ticket)
        raise OSError("ticket short write")
    seals = fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL
    fcntl.fcntl(ticket, fcntl.F_ADD_SEALS, seals)
    argv = [str(Path(sys.executable).resolve()), "-I", "-S", "-B", str(Path(__file__).resolve()), "--child", mode, str(root), str(ticket)]
    child = None; reaped = False; result = None
    def stop(signum, frame):
        raise TimeoutError("outer deadline")
    signal.signal(signal.SIGALRM, stop)
    signal.setitimer(signal.ITIMER_REAL, 2)
    try:
        child = subprocess.Popen(argv, env=ENV, cwd=Path(__file__).resolve().parent, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, close_fds=True, pass_fds=(ticket,))
        os.set_blocking(child.stdout.fileno(), False)
        output = bytearray()
        deadline = time.monotonic() + 1
        while time.monotonic() < deadline:
            ready, _, _ = select.select([child.stdout], [], [], .005)
            if ready:
                block = os.read(child.stdout.fileno(), 4096)
                output.extend(block)
                if len(output) > OUTPUTS["stdout.log"]:
                    raise ValueError("outer pipe cap")
                if not block:
                    break
        else:
            raise TimeoutError("fixed helper deadline")
        got, status, usage = os.wait4(child.pid, 0)
        reaped = got == child.pid
        child.returncode = os.waitstatus_to_exitcode(status)
        child.stdout.close()
        attempt.write("stdout.log", bytes(output))
        if not reaped or status != 0:
            raise ValueError("helper nonzero or missing wait4")
        result = json.loads(output)
        attempt.write("profile.json", _json(result["profile"]))
        cpu = time.process_time() + usage.ru_utime + usage.ru_stime
        wall = time.monotonic() - started
        if cpu + 3 > CPU or wall + .25 > WALL:
            raise ValueError("settlement/exit margin exhausted")
        attempt.write("manifest.json", _json({"slots": OUTPUTS, "held_bytes": sum(OUTPUTS.values()),
                                              "written_before_manifest": sorted(attempt.written)}))
        attempt.write("outer.receipt.json", _json({"status": "bounded complete; no successor", "argv": argv,
            "pid": child.pid, "wait_status": status, "wait4": True, "child_cpu": usage.ru_utime + usage.ru_stime,
            "child_rss_kib": usage.ru_maxrss, "observer_cpu_before_tail": time.process_time(),
            "wall_before_tail": wall, "tail_wall_bound": .25, "remaining_observer_cpu_bound": 3,
            "as_per_process": 134217728, "max_processes": 2, "products": 0, "workers": 0,
            "profile_sha256": _digest(_json(result["profile"])), "observer_profile": observer_profile}))
    except BaseException as error:
        try:
            attempt.write("partial.json", _json({"status": "UNKNOWN", "error": repr(error), "successor": False}))
        except BaseException:
            pass
        raise
    finally:
        os.close(ticket)
        if child is not None:
            try:
                if not reaped:
                    # Only our unreaped direct child; this fixed helper has no spawn path.
                    try:
                        os.kill(child.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    while True:
                        try:
                            got, status, usage = os.wait4(child.pid, 0)
                            child.returncode = os.waitstatus_to_exitcode(status)
                            break
                        except InterruptedError:
                            continue
            finally:
                child.stdout.close()


def main():
    arguments = sys.argv[1:]
    child = arguments[:1] == ["--child"]
    if child:
        arguments = arguments[1:]
        if len(arguments) != 3 or not arguments[2].isdecimal():
            raise ValueError("sealed outer ticket required")
        fd = int(arguments[2])
        seals = fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL
        if fcntl.fcntl(fd, fcntl.F_GET_SEALS) != seals:
            raise ValueError("sealed outer ticket required")
        token = json.loads(os.pread(fd, 1025, 0))
        wanted = {"parent": os.getppid(), "mode": arguments[0], "root": arguments[1],
                  "source": _digest(_read(Path(__file__).resolve()))}
        if token != wanted or not (Path(arguments[1]) / "outer.pending").is_file():
            raise ValueError("sealed outer ticket binding")
        os.close(fd)
        arguments = arguments[:2]
    if len(arguments) != 2 or arguments[0] not in ("profile", "verify", "init"):
        raise ValueError("fixed profile/verify/init only; worker/product/compiler disabled")
    mode, directory = arguments
    root = Path(directory).resolve()
    os.chdir(Path(__file__).resolve().parent)
    if child:
        _child(mode, root)
    else:
        _outer(mode, root)


if __name__ == "__main__":
    try:
        main()
    except BaseException as error:
        os.write(2, (repr(error)[:1024] + "\n").encode())
        os._exit(1)
    os._exit(0)

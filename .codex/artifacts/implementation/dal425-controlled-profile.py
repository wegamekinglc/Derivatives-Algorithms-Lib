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


class SourceLoader(_external.SourceFileLoader):
    def __init__(self, fullname, path, source=None):
        super().__init__(fullname, path)
        self.source = source

    def get_code(self, fullname):
        source = self.source
        if source is None:
            with open(self.path, "rb") as stream:
                source = stream.read(1048577)
        if len(source) > 1048576:
            raise ValueError("source cap")
        code = compile(source, self.path, "exec", dont_inherit=True, optimize=sys.flags.optimize)
        SOURCES[self.path] = (source, code)
        return code


class SourceFinder:
    def find_spec(self, fullname, path=None, target=None):
        spec = _external.PathFinder.find_spec(fullname, path, target)
        if spec and spec.origin and spec.origin.endswith(".py"):
            spec.loader = SourceLoader(fullname, spec.origin)
        elif spec and spec.origin and spec.origin.endswith(".pyc"):
            raise ValueError("standalone bytecode disabled")
        return spec


if __name__ == "__main__":
    sys.meta_path[sys.meta_path.index(_external.PathFinder)] = SourceFinder()

import fcntl
import hashlib
import io
import importlib.util
import json
import os
from pathlib import Path
import resource
import select
import signal
import stat
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


def _load_process():
    path = Path(__file__).with_name('dal425-controlled-process.py')
    loader = SourceLoader('dal425_profile_process', str(path))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    sys.modules[loader.name] = module
    loader.exec_module(module)
    return module


_process = _load_process()
_code_data = _process.code_data


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


def _initial_functions(name, module, expected):
    nested = _nested(expected)
    for value in vars(module).values():
        if getattr(value, "__module__", None) != name:
            continue
        code = getattr(value, "__code__", None)
        if code is not None and nested.get((code.co_qualname, code.co_firstlineno)) != code:
            raise ValueError("loaded initial function/source mismatch: " + name)


def _initial_source(name, module, origin):
    source = _read(origin)
    expected = compile(source, origin, "exec", dont_inherit=True, optimize=sys.flags.optimize)
    cache = getattr(module, "__cached__", None)
    cache_hash = None
    if cache and Path(cache).exists():
        cache_bytes = _read(cache)
        loader = _external.SourceFileLoader(name, origin)
        verify_loaded_code(loader.get_code(name), source, origin)
        cache_hash = _digest(cache_bytes)
    _initial_functions(name, module, expected)
    return {"origin": origin, "source": _digest(source), "cache": cache_hash,
            "selection": "source/cache code-equivalent; faithful fixed CPython loader assumed"}


def _initial_module(name):
    module = sys.modules[name]
    spec = getattr(module, "__spec__", None)
    origin = getattr(spec, "origin", None)
    if origin in (None, "built-in", "frozen"):
        return {"origin": origin}
    if origin.endswith(".so"):
        return {"origin": origin, "sha256": _digest(_read(origin, 16777216))}
    if not origin.endswith(".py"):
        raise ValueError("unbound initial module: " + name)
    return _initial_source(name, module, origin)


def _source_profiles():
    if len(SOURCES) > 128 or sum(len(value[0]) for value in SOURCES.values()) > 8388608 or len(EXECUTED) > 256:
        raise ValueError("fixed dependency cap")
    sources = {}
    for name, (source, code) in tuple(SOURCES.items()):
        verify_loaded_code(code, source, name)
        if _read(name) != source:
            raise ValueError("source changed after load")
        sources[name] = {"source": _digest(source), "executed_code": _digest(_json(_code_data(code))),
                         "selection": "forced source, cache bypassed"}
    return sources


def _mapped_profile(line):
    fields = line.split(maxsplit=5)
    if len(fields) != 6 or not fields[5].startswith("/"):
        return None
    file = Path(fields[5]); info = file.stat()
    if str(info.st_ino) != fields[4] or "%02x:%02x" % (os.major(info.st_dev), os.minor(info.st_dev)) != fields[3]:
        raise ValueError("mapped identity changed")
    return str(file), _digest(_read(file, 33554432))


def _mapped_profiles():
    mapped = {}
    for line in Path("/proc/self/maps").read_text().splitlines():
        entry = _mapped_profile(line)
        if entry is not None:
            name, digest = entry
            mapped[name] = digest
    if len(mapped) > 64 or sum(Path(name).stat().st_size for name in mapped) > 67108864:
        raise ValueError("mapped dependency cap")
    return mapped


def startup_profile():
    path = str(Path(__file__).resolve())
    data = _read(path)
    verify_loaded_code(BOOT_CODE, data, BOOT_CODE.co_filename)
    initial = {name: _initial_module(name) for name in INITIAL}
    sources = _source_profiles()
    mapped = _mapped_profiles()
    return {"schema": 1, "python": sys.version, "executable": str(Path(sys.executable).resolve()),
            "bootstrap": {"source": _digest(data), "executed_code": _digest(_json(_code_data(BOOT_CODE)))},
            "initial": initial, "sources": sources, "mapped": mapped,
            "executions": [_digest(_json(_code_data(code))) for code in tuple(EXECUTED)],
            "env": ENV, "cwd": str(Path.cwd()), "worker_enabled": False}


def _budget(budget, required):
    if (type(budget) is not dict or set(budget) != set(required)
            or any(type(budget[k]) is not int or budget[k] < v for k, v in required.items())):
        raise ValueError("outer budget exhausted or invalid")


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
        _budget(self.budget, required)
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
    loader = SourceLoader("dal425_profile_init", str(path), source)
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    module.__dict__.update(_SOURCE_BYTES=source, _FRESH_ENTRY=True)
    loader.exec_module(module)
    if SOURCES[str(path)][0] != source:
        raise ValueError("source changed after load")
    return vars(module)


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


def _ticket(mode, root, attempt):
    ticket = os.memfd_create("dal425-outer-ticket", os.MFD_ALLOW_SEALING | os.MFD_CLOEXEC)
    try:
        token = _json({"parent": os.getpid(), "mode": mode, "root": str(root),
                       "source": _digest(_read(Path(__file__).resolve()))})
        attempt.write("launch.ticket", token)
        if os.write(ticket, token) != len(token):
            raise OSError("ticket short write")
        seals = fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL
        fcntl.fcntl(ticket, fcntl.F_ADD_SEALS, seals)
        return ticket
    except BaseException:
        os.close(ticket)
        raise


def _observer_inputs(mode, root):
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
    return observer_profile, attempt, started


def _helper_argv(mode, root, ticket):
    return ['/usr/bin/python3', "-I", "-S", "-B", str(Path(__file__).resolve()), "--child", mode, str(root), str(ticket)]


def _helper_output(child):
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
                return bytes(output)
    raise TimeoutError("fixed helper deadline")


class _OuterHelper:
    def __init__(self, argv):
        self.argv = argv
        self.child = None
        self.reaped = False
        self.status = self.usage = None

    def collect(self, mode, root, ticket):
        if mode not in ("profile", "verify", "init") or self.argv != _helper_argv(mode, root, ticket):
            raise ValueError("fixed helper command required")
        self.child = _process.spawn_helper(self.argv, root, ticket)
        os.set_blocking(self.child.stdout.fileno(), False)
        output = _helper_output(self.child)
        got, self.status, self.usage = os.wait4(self.child.pid, 0)
        self.reaped = got == self.child.pid
        self.child.returncode = os.waitstatus_to_exitcode(self.status)
        self.child.stdout.close()
        return output

    def _stop(self):
        try:
            os.kill(self.child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass

    def _wait_cleanup(self):
        while True:
            try:
                got, self.status, self.usage = os.wait4(self.child.pid, 0)
                self.reaped = got == self.child.pid
                self.child.returncode = os.waitstatus_to_exitcode(self.status)
                return
            except InterruptedError:
                continue

    def cleanup(self):
        if self.child is None:
            return
        try:
            if not self.reaped:
                self._stop()
                self._wait_cleanup()
        finally:
            self.child.stdout.close()

    def receipt(self):
        if not self.reaped:
            return None
        return {"pid": self.child.pid, "wait_status": self.status,
                "child_cpu": self.usage.ru_utime + self.usage.ru_stime, "child_rss_kib": self.usage.ru_maxrss}


def _record_helper(attempt, helper, output, started, observer_profile):
    attempt.write("stdout.log", output)
    if not helper.reaped or helper.status != 0:
        raise ValueError("helper nonzero or missing wait4")
    result = json.loads(output)
    attempt.write("profile.json", _json(result["profile"]))
    usage = helper.usage
    cpu = time.process_time() + usage.ru_utime + usage.ru_stime
    wall = time.monotonic() - started
    if cpu + 3 > CPU or wall + .25 > WALL:
        raise ValueError("settlement/exit margin exhausted")
    attempt.write("manifest.json", _json({"slots": OUTPUTS, "held_bytes": sum(OUTPUTS.values()),
                                          "written_before_manifest": sorted(attempt.written)}))
    attempt.write("outer.receipt.json", _json({"status": "bounded complete; no successor", "argv": helper.argv,
        "pid": helper.child.pid, "wait_status": helper.status, "wait4": True, "child_cpu": usage.ru_utime + usage.ru_stime,
        "child_rss_kib": usage.ru_maxrss, "observer_cpu_before_tail": time.process_time(),
        "wall_before_tail": wall, "tail_wall_bound": .25, "remaining_observer_cpu_bound": 3,
        "as_per_process": 134217728, "max_processes": 2, "products": 0, "workers": 0,
        "profile_sha256": _digest(_json(result["profile"])), "observer_profile": observer_profile}))


def _outer(mode, root):
    observer_profile, attempt, started = _observer_inputs(mode, root)
    ticket = _ticket(mode, root, attempt)
    helper = _OuterHelper(_helper_argv(mode, root, ticket))
    def stop(signum, frame):
        raise TimeoutError("outer deadline")
    signal.signal(signal.SIGALRM, stop)
    signal.setitimer(signal.ITIMER_REAL, 2)
    try:
        output = helper.collect(mode, root, ticket)
        _record_helper(attempt, helper, output, started, observer_profile)
    except BaseException as error:
        try:
            attempt.write("partial.json", _json({"status": "UNKNOWN", "error": repr(error), "successor": False,
                                                  "wait4": helper.receipt()}))
        except BaseException:
            pass
        raise
    finally:
        os.close(ticket)
        helper.cleanup()


def _child_arguments(arguments):
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
    return arguments[:2]


def main():
    arguments = sys.argv[1:]
    child = arguments[:1] == ["--child"]
    if child:
        arguments = _child_arguments(arguments[1:])
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

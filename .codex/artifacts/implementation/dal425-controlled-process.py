import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import signal
from types import CodeType

ENV = {"LC_ALL": "C", "TZ": "UTC"}
BASE = Path(__file__).resolve().parent
SEALS = fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL
BOOTSTRAP = b'''import os, resource, sys
resource.setrlimit(resource.RLIMIT_AS, (134217728, 134217728))
hard = resource.getrlimit(resource.RLIMIT_CPU)[1]
hard = 3 if hard == resource.RLIM_INFINITY else min(hard, 3)
resource.setrlimit(resource.RLIMIT_CPU, (hard, hard))
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
import fcntl, hashlib, importlib.machinery, importlib.util, json
seals = fcntl.F_SEAL_WRITE | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_SEAL
for fd in (3, 4, 5):
    if fcntl.fcntl(fd, fcntl.F_GET_SEALS) != seals:
        raise ValueError('sealed dispatch input required')
raw = os.pread(4, 4097, 0)
if len(raw) > 4096:
    raise ValueError('dispatch payload cap')
cfg = json.loads(raw)
os.close(4)
allowed = {'worker':'dal425-controlled-execution.py', 'helper':'dal425-controlled-profile.py', 'initializer':'dal425-controlled-launch.py'}
if cfg['parent'] != os.getppid() or cfg['cwd'] != os.getcwd():
    raise ValueError('dispatch parent/cwd binding')
if os.path.basename(cfg['source']) != allowed[cfg['kind']]:
    raise ValueError('fixed dispatch source required')
source = os.pread(5, 65537, 0)
if len(source) > 65536 or hashlib.sha256(source).hexdigest() != cfg['source_sha256']:
    raise ValueError('sealed dispatch source mismatch')
sys.argv = [cfg['source'], *cfg['argv'][5:]]
if cfg['kind'] == 'helper':
    sys.argv[-1] = '6'
class Loader(importlib.machinery.SourceFileLoader):
    def get_code(self, fullname):
        code = compile(source, cfg['source'], 'exec', dont_inherit=True, optimize=0)
        os.close(5)
        return code
loader = Loader('__main__', cfg['source'])
spec = importlib.util.spec_from_loader(loader.name, loader)
module = importlib.util.module_from_spec(spec)
module.__dict__.update(_SOURCE_BYTES=source)
sys.modules['__main__'] = module
loader.exec_module(module)
'''
BOOTSTRAP_SHA256 = hashlib.sha256(BOOTSTRAP).hexdigest()


def native_argv():
    return ['/usr/bin/python3', '-I', '-S', '-B', '/proc/self/fd/3']


def recorded_argv(intent, payload_sha256):
    return dict(process=native_argv(), intent=intent, payload_sha256=payload_sha256,
                bootstrap_sha256=BOOTSTRAP_SHA256)


def _constant(value):
    if isinstance(value, CodeType):
        return code_data(value)
    if isinstance(value, (tuple, frozenset)):
        values = [_constant(item) for item in value]
        return [type(value).__name__, sorted(values, key=repr) if isinstance(value, frozenset) else values]
    return [type(value).__name__, repr(value)]


def code_data(code):
    return [code.co_code.hex(), [_constant(v) for v in code.co_consts], code.co_names,
            code.co_varnames, code.co_freevars, code.co_cellvars, code.co_flags,
            code.co_argcount, code.co_posonlyargcount, code.co_kwonlyargcount,
            code.co_name, code.co_qualname, code.co_filename, code.co_firstlineno,
            code.co_linetable.hex(), code.co_exceptiontable.hex()]


def _descriptor(fd):
    if type(fd) is not int or fd < 3 or fcntl.fcntl(fd, fcntl.F_GET_SEALS) != SEALS:
        raise ValueError("sealed fixed input descriptor required")


def _private_root(root):
    root = Path(root)
    info = root.lstat()
    if not root.is_absolute() or root.resolve() != root or not stat.S_ISDIR(info.st_mode):
        raise ValueError("fixed absolute root required")
    if info.st_uid != os.getuid() or info.st_mode & 0o077:
        raise ValueError("private root required")


def _inherited_fds():
    descriptors = []
    for value in os.listdir('/proc/self/fd'):
        fd = int(value)
        if fd < 3:
            continue
        try:
            fcntl.fcntl(fd, fcntl.F_GETFD)
        except OSError as error:
            if error.errno != errno.EBADF:
                raise
        else:
            descriptors.append(fd)
    return descriptors


def _file_actions(write_fd, null_fd, keep):
    bindings = keep if isinstance(keep, dict) else {fd: fd for fd in keep}
    actions = [(os.POSIX_SPAWN_DUP2, null_fd, 0), (os.POSIX_SPAWN_DUP2, write_fd, 1),
               (os.POSIX_SPAWN_DUP2, write_fd, 2)]
    actions.extend((os.POSIX_SPAWN_DUP2, source, target) for target, source in bindings.items())
    actions.extend((os.POSIX_SPAWN_CLOSE, fd) for fd in _inherited_fds() if fd not in bindings)
    return actions


class Child:
    def __init__(self, stdout):
        self.stdout = stdout
        self.pid = None
        self.returncode = None
        self.recorded_argv = None


def _sealed_bytes(name, data, owned, cap):
    if len(data) > cap:
        raise ValueError('sealed dispatch byte cap')
    fd = os.memfd_create(name, os.MFD_ALLOW_SEALING | os.MFD_CLOEXEC)
    owned.append(fd)
    if os.write(fd, data) != len(data):
        raise OSError('sealed dispatch short write')
    fcntl.fcntl(fd, fcntl.F_ADD_SEALS, SEALS)
    return fd


def _dispatch_source(argv, keep, owned):
    if len(argv) == 8 and argv[5] == '--worker':
        return 'worker', BASE / 'dal425-controlled-execution.py', keep[0]
    path = Path(argv[4])
    kinds = {'dal425-controlled-profile.py': 'helper', 'dal425-controlled-launch.py': 'initializer'}
    if path.parent != BASE or path.name not in kinds:
        raise ValueError('fixed dispatch source required')
    fd = _sealed_bytes('dal425-entry', path.read_bytes(), owned, 65536)
    return kinds[path.name], path, fd


def _native_inputs(argv, cwd, keep, owned):
    kind, path, source_fd = _dispatch_source(argv, keep, owned)
    source = os.pread(source_fd, 65537, 0)
    payload = json.dumps(dict(kind=kind, source=str(path), source_sha256=hashlib.sha256(source).hexdigest(),
        argv=argv, parent=os.getpid(), cwd=str(Path(cwd).resolve())), sort_keys=True, allow_nan=False).encode()
    bootstrap = _sealed_bytes('dal425-bootstrap', BOOTSTRAP, owned, 8192)
    config = _sealed_bytes('dal425-dispatch', payload, owned, 4096)
    originals = {3: bootstrap, 4: config, 5: source_fd}
    if kind == 'helper':
        originals[6] = keep[0]
    bindings = {}
    for target, source in originals.items():
        staged = fcntl.fcntl(source, fcntl.F_DUPFD_CLOEXEC, 16)
        owned.append(staged)
        bindings[target] = staged
    return bindings, recorded_argv(argv, hashlib.sha256(payload).hexdigest())


def _close_fds(descriptors):
    errors = []
    while descriptors:
        try:
            os.close(descriptors.pop())
        except OSError as error:
            errors.append(error)
    if errors:
        for error in errors[1:]:
            errors[0].add_note(str(error))
        raise errors[0]


def _abort_child(child, error):
    if child.stdout is not None:
        try:
            child.stdout.close()
        except OSError as close_error:
            error.add_note('pipe close: ' + str(close_error))
    if child.pid is not None:
        _reap_failed_spawn(child, error)


def _reap_failed_spawn(child, error):
    try:
        os.kill(child.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    while True:
        try:
            raw = os.wait4(child.pid, 0)
            error.add_note('owned spawn failure wait4: ' + repr((raw[0], raw[1], tuple(raw[2]))))
            return
        except InterruptedError:
            continue


def _spawn_pipe(argv, cwd, keep):
    if Path.cwd().resolve() != Path(cwd).resolve():
        raise ValueError("fixed spawn cwd required")
    child = Child(None)
    read_fd, write_fd = os.pipe2(os.O_CLOEXEC)
    owned = [read_fd, write_fd]
    try:
        child.stdout = os.fdopen(read_fd, 'rb', buffering=0)
        owned.remove(read_fd)
        null_fd = os.open('/dev/null', os.O_RDONLY | os.O_CLOEXEC)
        owned.append(null_fd)
        bindings, child.recorded_argv = _native_inputs(argv, cwd, keep, owned)
        # Keep the OS command literal; runtime data travels through sealed FDs.
        child.pid = os.posix_spawn('/usr/bin/python3',
            ['/usr/bin/python3', '-I', '-S', '-B', '/proc/self/fd/3'], {'LC_ALL': 'C', 'TZ': 'UTC'},
            file_actions=_file_actions(write_fd, null_fd, bindings), setsigmask=())
        _close_fds(owned)
        return child
    except BaseException as error:
        try:
            _close_fds(owned)
        except OSError as close_error:
            error.add_note('parent close: ' + str(close_error))
        _abort_child(child, error)
        raise


def spawn_worker(argv, root, source_fd):
    _descriptor(source_fd)
    _private_root(root)
    if len(argv) != 8 or argv[:6] != ['/usr/bin/python3', '-I', '-S', '-B', '/proc/self/fd/' + str(source_fd), '--worker']:
        raise ValueError("fixed worker command required")
    if argv[6] not in ('ok', 'nonzero', 'burn', 'flood') or not 0 < float(argv[7]) <= 2:
        raise ValueError("fixed worker action/CPU required")
    source = os.pread(source_fd, 65537, 0)
    if len(source) > 65536 or source != (BASE / 'dal425-controlled-execution.py').read_bytes():
        raise ValueError("fixed sealed worker source required")
    return _spawn_pipe(argv, root, (source_fd,))


def spawn_helper(argv, root, ticket):
    _descriptor(ticket)
    _private_root(root)
    if len(argv) != 9 or argv[:6] != ['/usr/bin/python3', '-I', '-S', '-B', str(BASE / 'dal425-controlled-profile.py'), '--child']:
        raise ValueError("fixed helper command required")
    if argv[6] not in ('profile', 'verify', 'init') or argv[7:] != [str(root), str(ticket)]:
        raise ValueError("fixed helper arguments required")
    token = os.pread(ticket, 1025, 0)
    expected = dict(parent=os.getpid(), mode=argv[6], root=str(root),
        source=hashlib.sha256((BASE / 'dal425-controlled-profile.py').read_bytes()).hexdigest())
    if len(token) > 1024 or json.loads(token) != expected:
        raise ValueError('sealed helper ticket binding')
    return _spawn_pipe(argv, BASE, (ticket,))


def spawn_initializer(root):
    _private_root(root)
    argv = ['/usr/bin/python3', '-I', '-S', '-B', str(BASE / 'dal425-controlled-launch.py'), 'init', str(root), 'ok']
    return _spawn_pipe(argv, Path.cwd(), ())

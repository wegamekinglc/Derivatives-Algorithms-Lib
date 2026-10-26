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
    actions = [(os.POSIX_SPAWN_DUP2, null_fd, 0), (os.POSIX_SPAWN_DUP2, write_fd, 1),
               (os.POSIX_SPAWN_DUP2, write_fd, 2)]
    actions.extend((os.POSIX_SPAWN_DUP2, fd, fd) for fd in keep)
    actions.extend((os.POSIX_SPAWN_CLOSE, fd) for fd in _inherited_fds() if fd not in keep)
    return actions


class Child:
    def __init__(self, stdout):
        self.stdout = stdout
        self.pid = None
        self.returncode = None


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
        child.pid = os.posix_spawn('/usr/bin/python3', argv, ENV,
            file_actions=_file_actions(write_fd, null_fd, keep), setsigmask=())
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

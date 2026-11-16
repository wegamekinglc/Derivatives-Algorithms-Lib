"""Supervise only the fixed local qualification programs without a shell."""

import os
import signal
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BINARY_ROOT = REPO / "build/dal-542-reference"
NATIVE = dict(
    capture="dal_market_iv_capture",
    mc="dal_market_iv_a1_mc",
    probe="dal_market_iv_a1_probe",
)


def program(kind):
    if kind in NATIVE:
        path = BINARY_ROOT / NATIVE[kind]
    elif kind == "multica":
        path = Path(os.environ["MULTICA_CLI_PATH"])
    elif kind in ("git", "compiler"):
        path = Path("/usr/bin/git" if kind == "git" else "/usr/bin/c++")
    else:
        raise ValueError("Program is not in the qualification allowlist")
    if not path.is_absolute() or not path.is_file():
        raise ValueError("Qualification program must be an existing absolute file")
    return path


class Child:
    def __init__(self, pid):
        self.pid = pid
        self.returncode = None

    def poll(self):
        if self.returncode is None:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                self.returncode = os.waitstatus_to_exitcode(status)
        return self.returncode

    def wait(self):
        if self.returncode is None:
            _, status = os.waitpid(self.pid, 0)
            self.returncode = os.waitstatus_to_exitcode(status)
        return self.returncode

    def terminate(self):
        os.kill(self.pid, signal.SIGTERM)


def data_path(value):
    path = Path(value).resolve()
    if not path.is_relative_to(REPO / "build"):
        raise ValueError("Process data must stay inside the qualification build")
    return path


def capture_arguments(args):
    if len(args) != 3 or args[-1] != "--full-precision":
        raise ValueError("Invalid fixed capture arguments")
    request, result = map(data_path, args[:2])
    if (
        request.name != "request.json"
        or result.name != "capture.json"
        or request.parent != result.parent
    ):
        raise ValueError("Invalid capture namespace")


def checkpoint_argument(value, parent, name, exponent):
    checkpoint = data_path(value)
    expected = name + "-tier-" + str(exponent - 1) + ".json.checkpoint.json"
    if checkpoint.parent != parent or checkpoint.name != expected:
        raise ValueError("Invalid MC checkpoint identity")


def mc_arguments(args):
    if len(args) not in (4, 5) or args[2] not in ("F2_linear", "F2_nonlinear"):
        raise ValueError("Invalid MC operation")
    exponent = int(args[3])
    if not 16 <= exponent <= 20:
        raise ValueError("Unregistered MC tier")
    request, result = map(data_path, args[:2])
    if request.name != "request.json" or result.parent != request.parent:
        raise ValueError("Invalid MC namespace")
    if result.name != args[2] + "-tier-" + str(exponent) + ".json":
        raise ValueError("Invalid MC result identity")
    if len(args) == 5:
        checkpoint_argument(args[4], request.parent, args[2], exponent)


def validate_arguments(kind, args):
    metadata = dict(
        git=[
            ["-C", str(REPO), "status", "--porcelain"],
            ["-C", str(REPO), "rev-parse", "HEAD"],
            ["-C", str(REPO), "submodule", "status", "--recursive"],
        ],
        compiler=[["--version"]],
        multica=[["daemon", "status", "--output", "json"]],
    )
    if kind in metadata:
        if args not in metadata[kind]:
            raise ValueError("Only fixed read-only metadata operations are allowed")
        return
    if kind == "capture":
        capture_arguments(args)
    elif kind == "mc":
        mc_arguments(args)
    elif len(args) != 1 or data_path(args[0]).name != "probe-input.txt":
        raise ValueError("Invalid probe input")


def launch(kind, arguments, stdout, stderr=None):
    program(kind)
    arguments = list(map(str, arguments))
    validate_arguments(kind, arguments)
    if Path.cwd().resolve() != REPO.resolve():
        raise ValueError("Qualification dispatcher must run from the repository")
    payload = job_data(kind, arguments)
    errors = stderr if stderr is not None else stdout
    read_fd, write_fd = os.pipe()
    actions = [
        (os.POSIX_SPAWN_DUP2, read_fd, 0),
        (os.POSIX_SPAWN_DUP2, stdout.fileno(), 1),
        (os.POSIX_SPAWN_DUP2, errors.fileno(), 2),
    ]
    try:
        pid = os.posix_spawn(
            "/bin/bash",
            [
                "/bin/bash",
                "--noprofile",
                "--norc",
                "tests/market-iv-local-vol/a1_dispatch.sh",
            ],
            {
                "PATH": "/usr/bin:/bin",
                "LC_ALL": "C",
                "OPENBLAS_NUM_THREADS": "1",
                "OMP_NUM_THREADS": "1",
            },
            file_actions=actions,
        )
    except BaseException:
        os.close(read_fd)
        os.close(write_fd)
        raise
    os.close(read_fd)
    child = Child(pid)
    try:
        with os.fdopen(write_fd, "wb") as stream:
            stream.write(payload)
    except BaseException:
        child.wait()
        raise
    return child


def runtime_environment():
    return {
        k: v
        for k, v in os.environ.items()
        if k in ("HOME", "USER") or k.startswith("MULTICA_")
    }


def job_data(kind, arguments):
    environment = runtime_environment() if kind == "multica" else {}
    fields = [kind, str(len(environment))]
    for key, value in environment.items():
        fields.extend((key, value))
    fields.extend((str(len(arguments)), *arguments))
    if any("\0" in value for value in fields):
        raise ValueError("NUL is not valid job data")
    payload = ("\0".join(fields) + "\0").encode()
    if len(payload) > 131072:
        raise ValueError("Oversized qualification job data")
    return payload


def output(kind, arguments):
    with tempfile.TemporaryFile(dir=REPO / "build") as stdout:
        with tempfile.TemporaryFile(dir=REPO / "build") as stderr:
            child = launch(kind, arguments, stdout, stderr)
            if child.wait() != 0:
                raise RuntimeError("Qualification metadata/probe command failed")
            stdout.seek(0)
            return stdout.read().decode("utf-8")


def native_kind(path):
    matches = [
        kind for kind in NATIVE if program(kind).resolve() == Path(path).resolve()
    ]
    if len(matches) != 1:
        raise ValueError("Native executable is outside the fixed qualification build")
    return matches[0]

"""Validate typed jobs and monitor the foreground driver's owned processes."""

import json
import os
import signal
import sys
import tempfile
import time
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


def process_start(pid):
    path = Path(f"/proc/{pid}/stat")
    if not path.exists():
        return None
    return path.read_text().rsplit(")", 1)[1].split()[19]


def recorded_start(path):
    text = path.read_text() if path.exists() else ""
    return text.rsplit(")", 1)[1].split()[19] if ")" in text else None


class Child:
    def __init__(self, folder, stdout, stderr):
        self.folder = folder
        self.stdout = stdout
        self.stderr = stderr
        self.pid = int((folder / "pid").read_text())
        record = folder / "start.stat"
        self.start = recorded_start(record)
        self.returncode = None

    def poll(self):
        status = self.folder / "status"
        if self.returncode is None and status.exists():
            self.returncode = int(status.read_text())
            self.stdout.write(
                (self.folder / "stdout").read_bytes().decode()
                if "b" not in self.stdout.mode
                else (self.folder / "stdout").read_bytes()
            )
            errors = (self.folder / "stderr").read_bytes()
            self.stderr.write(
                errors.decode() if "b" not in self.stderr.mode else errors
            )
            self.stdout.flush()
            self.stderr.flush()
        return self.returncode

    def wait(self):
        while self.poll() is None:
            time.sleep(0.02)
        return self.returncode

    def terminate(self):
        if self.start is not None and process_start(self.pid) == self.start:
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


def validated_root(path):
    root = Path(path).resolve()
    info = root.stat()
    if (
        not root.is_relative_to(REPO / "build")
        or info.st_uid != os.getuid()
        or info.st_mode & 0o077
    ):
        raise ValueError("Invalid private process broker")
    return root


def broker_root():
    return validated_root(os.environ["DAL_A1_BROKER_DIR"])


def launch(kind, arguments, stdout, stderr=None):
    program(kind)
    arguments = list(map(str, arguments))
    validate_arguments(kind, arguments)
    root = broker_root()
    payload = job_data(kind, arguments)
    folder = Path(tempfile.mkdtemp(prefix="job-", dir=root))
    (folder / "request").write_bytes(payload)
    descriptor = os.open(root / "queue", os.O_WRONLY | os.O_NONBLOCK)
    try:
        os.write(descriptor, (str(folder) + "\n").encode())
    finally:
        os.close(descriptor)
    while not (folder / "pid").exists():
        if (folder / "status").exists():
            raise RuntimeError("Process broker failed before PID handoff")
        time.sleep(0.02)
    return Child(folder, stdout, stderr if stderr is not None else stdout)


def job_data(kind, arguments):
    fields = [kind, "0", str(len(arguments)), *arguments]
    if any("\0" in value for value in fields):
        raise ValueError("NUL is not valid job data")
    payload = ("\0".join(fields) + "\0").encode()
    if len(payload) > 131072:
        raise ValueError("Oversized qualification job data")
    return payload


def daemon_pids(value):
    if not isinstance(value, dict):
        return set()
    result = {v for k, v in value.items() if "pid" in k.lower() and isinstance(v, int)}
    for child in value.values():
        result.update(daemon_pids(child))
    return result


def cleanup(root):
    root = validated_root(root)
    forbidden = daemon_pids(json.loads((root / "daemon-status.json").read_text()))
    for folder in root.glob("job-*"):
        if (folder / "status").exists() or not (folder / "pid").exists():
            continue
        pid = int((folder / "pid").read_text())
        start = folder / "start.stat"
        recorded = recorded_start(start)
        if (
            pid not in forbidden
            and recorded is not None
            and process_start(pid) == recorded
        ):
            os.kill(pid, signal.SIGTERM)


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


if __name__ == "__main__":
    cleanup(Path(sys.argv[1]))

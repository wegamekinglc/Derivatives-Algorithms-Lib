"""Sequential native execution with explicit, retained retries."""

from contextlib import contextmanager
import json
from pathlib import Path
import subprocess  # nosec B404: execute only the identity-verified native study image
import time

from lsmc_study_build import digest, require_source_inputs
from lsmc_study_data import parse_record
from lsmc_study_store import archive_attempt, load_attempt, require_complete, save_attempt


def verify_image(identity):
    require_source_inputs(Path(identity["source"]), identity["source_sha256"])
    paths = dict(identity["reused_sha256"])
    paths[identity["stock_binary"]] = identity["stock_sha256"]
    paths[identity["executable"]] = identity["executable_sha256"]
    control = Path(identity["executable"]).parent
    paths[str(control / "address-control.patch")] = identity["patch_sha256"]
    paths[str(control / "lsmc.cpp")] = identity["controlled_source_sha256"]
    if any(digest(path) != expected for path, expected in paths.items()):
        raise ValueError("study source, executable or reused build input identity changed")


@contextmanager
def dataset_lock(root):
    import fcntl

    with (Path(root) / "run.lock").open("a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ValueError("this dataset already has an active runner") from error
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)


def existing_record(root, job, anchor, retry):
    try:
        attempt = load_attempt(root, job)
        return None if attempt is None else parse_record(attempt["stdout"], job, anchor)
    except (ValueError, KeyError, json.JSONDecodeError):
        if not retry:
            raise
        archive_attempt(root, job)
        return None


def output_text(value):
    return value.decode(errors="replace") if isinstance(value, bytes) else value or ""


def execute_job(root, job, executable, timeout, process):
    command = job.command(executable)
    start = time.monotonic()
    try:
        result = process(command, capture_output=True, text=True, timeout=timeout, check=False, shell=False)
        stdout, stderr, returncode = result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired as error:
        stdout, stderr, returncode = output_text(error.stdout), output_text(error.stderr), 124
    except OSError as error:
        stdout, stderr, returncode = "", str(error), 127
    save_attempt(root, job, command, stdout, stderr, returncode, time.monotonic() - start)


def run_jobs(root, jobs, anchor, executable, timeout=900, retry=False, process=subprocess.run):
    last_progress = time.monotonic()
    for index, job in enumerate(jobs, 1):
        if existing_record(root, job, anchor, retry) is None:
            execute_job(root, job, executable, timeout, process)
            attempt = load_attempt(root, job)
            parse_record(attempt["stdout"], job, anchor)
        now = time.monotonic()
        if now - last_progress >= 30 or index == len(jobs):
            print(f"completed {index}/{len(jobs)}: {job.case} N={job.training} {job.mode}", flush=True)
            last_progress = now
    require_complete(root, jobs)

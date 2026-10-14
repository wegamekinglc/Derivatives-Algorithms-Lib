"""Immutable study manifests and retained process-attempt evidence."""

from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import uuid


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("x") as output:
        json.dump(value, output, indent=2, allow_nan=False)
        output.write("\n")
    temporary.replace(path)


def prepare_dataset(root, context, jobs):
    root = Path(root)
    wanted = {"context": context, "jobs": [asdict(job) for job in jobs]}
    path = root / "profile.json"
    if path.exists():
        if json.loads(path.read_text()) != wanted:
            raise ValueError("existing dataset has a different image, settings or job matrix")
        return
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()):
        raise ValueError("refusing to initialize a nonempty evidence directory")
    (root / "samples").mkdir()
    write_json(path, wanted)


def attempt_paths(root, job):
    base = Path(root) / "samples" / job.key
    return {kind: base.with_suffix("." + kind) for kind in ("stdout", "stderr", "json")}


def text_hash(value):
    return hashlib.sha256(value.encode()).hexdigest()


def save_attempt(root, job, command, stdout, stderr, returncode, elapsed):
    paths = attempt_paths(root, job)
    if any(path.exists() for path in paths.values()):
        raise FileExistsError("preserve the existing process attempt before retrying")
    paths["stdout"].write_text(stdout)
    paths["stderr"].write_text(stderr)
    write_json(paths["json"], dict(job=asdict(job), command=command, returncode=returncode, elapsed_seconds=elapsed,
                                   stdout_sha256=text_hash(stdout), stderr_sha256=text_hash(stderr)))


def load_attempt(root, job):
    paths = attempt_paths(root, job)
    if not any(path.exists() for path in paths.values()):
        return None
    if not all(path.is_file() for path in paths.values()):
        raise ValueError("incomplete process attempt; preserve it before an explicit retry")
    receipt = json.loads(paths["json"].read_text())
    if receipt["job"] != asdict(job) or receipt["returncode"] != 0:
        raise ValueError("failed or mismatched process attempt; explicit retry required")
    stdout, stderr = paths["stdout"].read_text(), paths["stderr"].read_text()
    if text_hash(stdout) != receipt["stdout_sha256"] or text_hash(stderr) != receipt["stderr_sha256"]:
        raise ValueError("raw process output identity changed")
    return dict(receipt, stdout=stdout, stderr=stderr)


def archive_attempt(root, job):
    destination = Path(root) / "failed-attempts" / (job.key + "-" + uuid.uuid4().hex)
    destination.mkdir(parents=True, exist_ok=False)
    for kind, path in attempt_paths(root, job).items():
        if path.exists():
            path.rename(destination / ("attempt." + kind))
    temporary = attempt_paths(root, job)["json"].with_suffix(".json.tmp")
    if temporary.exists():
        temporary.rename(destination / "attempt.json.tmp")
    return destination


def require_complete(root, jobs):
    expected = {job.key for job in jobs}
    actual = {path.stem for path in (Path(root) / "samples").glob("*.json")}
    if len(expected) != len(jobs) or actual != expected:
        raise ValueError("incomplete, duplicate or unexpected study samples")
    return tuple(load_attempt(root, job) for job in jobs)

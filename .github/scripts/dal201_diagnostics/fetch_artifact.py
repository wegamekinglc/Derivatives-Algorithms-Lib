"""Download one fixed, original Actions artifact using the read-only job token."""

import argparse
import json
from pathlib import Path
import stat
import subprocess
import zipfile

from diagnostics import REPOSITORY, load_lock, verify_hash, write_json


def verify_metadata(metadata, lock):
    if (metadata["id"] != lock["artifact_id"] or metadata["name"] != lock["artifact_name"]
            or metadata["expired"] or metadata["workflow_run"]["id"] != lock["run_id"]
            or metadata["workflow_run"]["head_sha"] != lock["published_sha"]
            or metadata["digest"] != "sha256:" + lock["artifact_zip_sha256"]):
        raise ValueError("original artifact ID/name/run/source/digest/expiry mismatch")


def extract_zip(archive, destination):
    with zipfile.ZipFile(archive) as source:
        names = set()
        if sum(member.file_size for member in source.infolist()) > 2 * 1024**3:
            raise ValueError("artifact exceeds 2 GiB expanded budget")
        for member in source.infolist():
            path = Path(member.filename)
            if (path.is_absolute() or ".." in path.parts or member.filename in names
                    or stat.S_ISLNK(member.external_attr >> 16)):
                raise ValueError(f"unsafe artifact member: {member.filename}")
            names.add(member.filename)
        destination.mkdir(parents=True, exist_ok=False)
        source.extractall(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    lock = load_lock()
    state = {"schema": "dal201.download/1", "status": "running"}
    try:
        endpoint = f"repos/{REPOSITORY}/actions/artifacts/{lock['artifact_id']}"
        response = subprocess.run(["gh", "api", endpoint], capture_output=True, timeout=60)
        (args.output / "artifact-api.stdout").write_bytes(response.stdout)
        (args.output / "artifact-api.stderr").write_bytes(response.stderr)
        response.check_returncode()
        metadata = json.loads(response.stdout)
        verify_metadata(metadata, lock)
        archive = args.output / "original-artifact.zip"
        with archive.open("wb") as output, (args.output / "download.stderr").open("wb") as error:
            subprocess.run(["gh", "api", endpoint + "/zip"], stdout=output, stderr=error, check=True, timeout=180)
        verify_hash(archive, lock["artifact_zip_sha256"])
        extract_zip(archive, args.output / "original")
        state["status"] = "downloaded"
        return 0
    except Exception as error:
        state.update(status="failed", error=f"{type(error).__name__}: {error}")
        return 1
    finally:
        write_json(args.output / "download.json", state)


if __name__ == "__main__":
    raise SystemExit(main())

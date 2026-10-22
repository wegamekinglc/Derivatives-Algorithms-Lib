"""Check every exported source/dependency blob against the tested merge."""

import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
snapshot = json.loads((evidence / "integration.json").read_text())
root = Path(snapshot["native_root"]) / "source"
records, eol, skipped = [], [], []


def audit(repository, revision, prefix=""):
    listing = subprocess.check_output(["git", "ls-tree", "-rz", revision], cwd=repository)
    for entry in listing.split(b"\0"):
        if not entry:
            continue
        metadata, name = entry.split(b"\t", 1)
        mode, kind, expected = metadata.decode().split()
        relative = prefix + name.decode()
        if kind != "blob":
            skipped.append(relative)
            continue
        data = (root / relative).read_bytes()
        actual = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if actual != expected:
            normalized = data.replace(b"\r\n", b"\n")
            assert hashlib.sha1(b"blob " + str(len(normalized)).encode() + b"\0" + normalized).hexdigest() == expected, relative
            assert Path(relative).suffix in (".bat", ".ps1"), relative
            eol.append(relative)
        records.append([relative, expected, hashlib.sha256(data).hexdigest()])


audit(repo, snapshot["head"])
for name, dependency in snapshot["dependencies"].items():
    relative = "dal-cpp/externals/" + name
    audit(repo / relative, dependency["pin"], relative + "/")
result = {"head": snapshot["head"], "verified_git_blobs": len(records), "content_mismatches": [], "eol_only": eol,
          "gitlinks": skipped, "aggregate_sorted_records_sha256": hashlib.sha256(json.dumps(sorted(records)).encode()).hexdigest()}
(evidence / "source-audit.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))

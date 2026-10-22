import hashlib
import json
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[4]
evidence = Path(__file__).resolve().parent
source = Path("/mnt/d/multica-builds/dal419-3103ab04/source")
mode = sys.argv[1]
snapshot = json.loads((evidence / "snapshot.json").read_text())
changed = {"dal-cpp/dal/script/visitor/debugger.hpp", "tests/installed-consumer/main.cpp"} if mode == "green" else set()
records = {}
eol_only = []
unused_gitlinks = []


def audit(repository, revision, prefix=""):
    tree = subprocess.check_output(["git", "ls-tree", "-rz", revision], cwd=repository)
    for entry in tree.split(b"\0"):
        if not entry:
            continue
        metadata, name = entry.split(b"\t")
        _, kind, oid = metadata.decode().split()
        relative = prefix + name.decode()
        if kind == "commit":
            if (repo / relative / ".git").exists():
                audit(repo / relative, oid, relative + "/")
            else:
                unused_gitlinks.append({"path": relative, "pin": oid})
            continue
        data = (source / relative).read_bytes()
        actual = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if relative in changed:
            assert data == (repo / relative).read_bytes(), relative
        elif actual != oid and Path(relative).suffix in {".bat", ".ps1"}:
            original = subprocess.check_output(["git", "show", oid], cwd=repository)
            assert original.replace(b"\r\n", b"\n") == data.replace(b"\r\n", b"\n"), relative
            eol_only.append(relative)
        else:
            assert actual == oid, (relative, actual, oid)
        records[relative] = {"master_blob": oid, "export_blob": actual, "sha256": hashlib.sha256(data).hexdigest()}


audit(repo, snapshot["master"])
result = {"mode": mode, "master": snapshot["master"], "checked": len(records), "mismatches": [], "eol_only": eol_only,
          "unused_gitlinks": unused_gitlinks, "files": records}
(evidence / f"source-audit-{mode}.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({key: value for key, value in result.items() if key != "files"}))

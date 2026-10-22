import hashlib
import json
import pathlib
import subprocess
import zipfile
import xml.etree.ElementTree as ET

out = pathlib.Path(__file__).resolve().parent
repo = out.parents[3]
author = repo / ".codex/artifacts/implementation/dal-418-evidence"
raw = zipfile.ZipFile(author / "raw-evidence.zip")
manifest = json.loads((author / "final-manifest.json").read_text())
print("Manifest keys:", list(manifest))
checks = []
for name, digest in {**manifest["sha256"], **manifest["evidence_sha256"]}.items():
    candidate = repo / name
    if candidate.is_file():
        data = candidate.read_bytes()
    elif (author / name).is_file():
        data = (author / name).read_bytes()
    elif name in raw.namelist():
        data = raw.read(name)
    else:
        raise AssertionError(("missing manifest input", name))
    actual = hashlib.sha256(data).hexdigest()
    assert actual == digest, (name, digest, actual)
    checks.append(name)
assert hashlib.sha256((author / "raw-evidence.zip").read_bytes()).hexdigest() == manifest["raw_archive_sha256"]
summaries = {}
for name in ("parent-red.xml", "master-red.xml", "latest-master-red.xml", "parent-expanded-red.xml", "target-green.xml"):
    root = ET.fromstring(raw.read(name))
    summaries[name] = {"tests": root.get("tests"), "failures": root.get("failures"),
                       "cases": [{"name": case.get("name"), "failure": " ".join(f.itertext()) if (f := case.find("failure")) is not None else None}
                                 for case in root.findall(".//testcase")]}
    if name in ("parent-red.xml", "master-red.xml", "latest-master-red.xml"):
        assert root.get("tests") == "3" and root.get("failures") == "3"
        for case in root.findall(".//testcase"):
            message = " ".join(case.find("failure").itertext())
            assert "InvalidScriptInstrumentDefinition" in message and "Not a valid number string" in message
    if name == "target-green.xml":
        assert root.get("tests") == "121" and root.get("failures") == "0"

def snapshots(name):
    root = ET.fromstring(raw.read(name))
    case = root.find(".//testcase[@name='TestAccrualCompatibilitySnapshots']")
    assert case is not None and case.find("failure") is None
    return {p.get("name"): p.get("value") for p in case.findall("./properties/property")}

parent = snapshots("parent-snapshots.xml")
green = snapshots("target-green.xml")
assert len(parent) == 504 and parent == green
assert json.loads((author / "parent-snapshots.json").read_text()) == parent
old = json.loads((author / "master-manifest-original.json").read_text())
fixed = json.loads((author / "master-manifest.json").read_text())
old_hash = old["sha256"]["dal-cpp/tests/model/test_gsr_rateoption_script.cpp"]
new_hash = fixed["sha256"]["dal-cpp/tests/model/test_gsr_rateoption_script.cpp"]
test_blob = subprocess.check_output(["git", "show", fixed["test_commit"] + ":dal-cpp/tests/model/test_gsr_rateoption_script.cpp"], cwd=repo)
isolated = (repo / "build/dal418-master/source/dal-cpp/tests/model/test_gsr_rateoption_script.cpp").read_bytes()
assert isolated == test_blob and hashlib.sha256(isolated).hexdigest() == new_hash
old_without = json.loads(json.dumps(old))
old_without["sha256"]["dal-cpp/tests/model/test_gsr_rateoption_script.cpp"] = new_hash
old_without["provenance_correction"] = fixed["provenance_correction"]
assert old_without == fixed
latest = json.loads((author / "latest-master-manifest.json").read_text())
for name, digest in latest["sha256"].items():
    assert hashlib.sha256((repo / name).read_bytes()).hexdigest() == digest, name
parent_manifest = json.loads((author / "parent-manifest.json").read_text())
parent_production = subprocess.check_output(["git", "show", parent_manifest["production_parent"] + ":dal-cpp/dal/model/gsrrateoption.cpp"], cwd=repo)
assert hashlib.sha256(parent_production).hexdigest() == parent_manifest["sha256"]["dal-cpp/dal/model/gsrrateoption.cpp"]
assert hashlib.sha256(test_blob).hexdigest() == parent_manifest["sha256"]["dal-cpp/tests/model/test_gsr_rateoption_script.cpp"]
status = subprocess.check_output(["git", "status", "--porcelain=v1", "-uno"], cwd=repo, text=True)
assert not status
summary = {"verified_manifest_entries": len(checks), "verified_raw_archive": True,
           "snapshots": {"parent": "9c547ab64ff2a53ef444ac928440e443fc720c24", "records": 504, "equal": True},
           "manifest_correction": {"original_hash": old_hash, "corrected_hash": new_hash,
                                   "test_commit": fixed["test_commit"], "isolated_matches_git_blob": True,
                                   "only_test_hash_and_correction_note_changed": True},
           "latest_master_red_inputs_verified": len(latest["sha256"]),
           "parent_input_blobs_verified": True, "xml": summaries}
(out / "author-audit.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps({k: v for k, v in summary.items() if k != "xml"}, indent=2))

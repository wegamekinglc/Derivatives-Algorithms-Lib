"""Compare byte-preserving records captured before and after the fix."""

import hashlib
import json
import pathlib
import xml.etree.ElementTree as ET
import zipfile

evidence = pathlib.Path(__file__).resolve().parent


def properties(name):
    if (evidence / name).exists():
        root = ET.parse(evidence / name).getroot()
    else:
        with zipfile.ZipFile(evidence / "raw-evidence.zip") as archive:
            root = ET.fromstring(archive.read(name))
    case = root.find(".//testcase[@name='TestAccrualCompatibilitySnapshots']")
    assert case is not None and case.find("failure") is None
    return {p.attrib["name"]: p.attrib["value"] for p in case.findall("./properties/property")}


parent = properties("parent-snapshots.xml")
current = properties("target-green.xml")
assert len(parent) == 36 * 14
assert parent == current
payload = json.dumps(parent, ensure_ascii=False, indent=2) + "\n"
(evidence / "parent-snapshots.json").write_text(payload)
summary = {"cases": 36, "equal_properties": len(parent), "identities_per_case": 7,
           "byte_equal": ["source row keys/text", "portable JSON"],
           "parent_snapshot_sha256": hashlib.sha256(payload.encode()).hexdigest()}
(evidence / "snapshot-comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary, indent=2))

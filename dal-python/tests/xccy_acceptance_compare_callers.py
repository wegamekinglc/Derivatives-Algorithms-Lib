"""Compare the recorded canonical caller payloads without rerunning MC."""

import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def payload_property(prop):
    value = prop.get("value", "")
    if not value.startswith("{"):
        return None
    data = json.loads(value)
    if data.get("schema") != "dal.xccy-gaussian-public-result/1":
        return None
    return data["phase"], value


def selected_case(case):
    name = case.get("name", "")
    if "CompleteCorePayload" not in name and "test_acceptance_core_parity" not in name:
        return False
    if any(case.find(tag) is not None for tag in ("failure", "error", "skipped")):
        raise ValueError("caller case did not pass")
    return True


def payloads(filename):
    result = {}
    for case in ET.parse(filename).iter("testcase"):
        if not selected_case(case):
            continue
        for prop in case.findall("properties/property"):
            item = payload_property(prop)
            if item is None:
                continue
            phase, value = item
            if phase in result:
                raise ValueError("duplicate caller phase")
            result[phase] = value
    if set(result) != {"Fit", "Validation", "HeldOut"}:
        raise ValueError("missing canonical caller phase")
    return result


def compare(directory):
    files = {"public": "final-public.xml", "excel": "final-excel.xml", "python": "new-python.xml"}
    callers = {caller: payloads(directory / name) for caller, name in files.items()}
    for phase in callers["public"]:
        if callers["public"][phase] != callers["excel"][phase] or callers["public"][phase] != callers["python"][phase]:
            raise ValueError("caller payload mismatch: " + phase)
    return {"complete_payload_bytes_equal": True, "callers": list(files),
            "payloads": {phase: json.loads(value) for phase, value in callers["public"].items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    result = compare(args.directory)
    (args.directory / "cross-interface.json").write_text(json.dumps(result, indent=2) + "\n")
    print("3/3 complete payloads byte-identical across public C++, Python and portable Excel")


if __name__ == "__main__":
    main()

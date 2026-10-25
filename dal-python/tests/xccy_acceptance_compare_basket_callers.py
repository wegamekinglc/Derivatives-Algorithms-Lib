"""Compare complete captured basket payloads from three independently run callers."""

import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET


PHASES = {"Fit", "Validation", "HeldOut"}
FILES = {"public": "new-public.xml", "excel": "new-excel.xml", "python": "new-python.xml"}


def selected_case(case):
    name = case.get("name", "")
    return name == "TestCompleteDatedMappingsAcrossPhases" or name.startswith("test_acceptance_basket_complete_dated_mappings[")


def captured_values(filename):
    for case in ET.parse(filename).iter("testcase"):
        if not selected_case(case):
            continue
        if any(case.find(tag) is not None for tag in ("failure", "error", "skipped")):
            raise ValueError("canonical caller case did not pass")
        for prop in case.findall("properties/property"):
            value = prop.get("value", "")
            if not value.startswith("{"):
                continue
            yield value


def payloads(filename):
    result = {}
    for value in captured_values(filename):
        data = json.loads(value)
        if data.get("schema") != "dal.xccy-gaussian-public-basket-result/1":
            raise ValueError("wrong basket schema")
        phase = data["allocation"]["phase"]
        if phase not in PHASES or phase in result:
            raise ValueError("unexpected or duplicate caller phase")
        if data["status"] != "Estimate" or data["basket_complete"] is not True:
            raise ValueError("incomplete basket result")
        result[phase] = value
    if set(result) != PHASES:
        raise ValueError("missing canonical caller phase")
    return result


def compare(directory):
    callers = {caller: payloads(directory / name) for caller, name in FILES.items()}
    for phase in PHASES:
        values = [payload[phase] for payload in callers.values()]
        if len(set(values)) != 1:
            raise ValueError("complete caller payload mismatch: " + phase)
    return {"complete_payload_bytes_equal": True, "callers": list(FILES),
            "payloads": {phase: json.loads(value) for phase, value in callers["public"].items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    directory = parser.parse_args().directory
    (directory / "cross-interface.json").write_text(json.dumps(compare(directory), indent=2) + "\n")
    print("3/3 full basket payloads byte-identical across public C++, Python and portable Excel")


if __name__ == "__main__":
    main()

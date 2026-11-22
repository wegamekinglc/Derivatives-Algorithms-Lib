"""Compare row captures from the same Windows SDK, Python extension and Excel XLL build."""

import argparse
import json
import math
from pathlib import Path


def scalar(text):
    try:
        return json.loads(text)
    except ValueError:
        return text


def capture(path, sdk=False):
    cases = {}
    black = None
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        fields = line.lstrip(".").split(maxsplit=1)
        if fields[:1] == ["BLACK"]:
            black = [float(x) for x in fields[1].split()]
        if fields[:1] != ["REFIT"]:
            continue
        case, body = fields[1].split(maxsplit=1)
        if sdk:
            pairs = (field.split("=", 1) for field in body.split("\t"))
            cases.setdefault(case, []).append({key: scalar(value) for key, value in pairs})
        else:
            cases[case] = json.loads(body)
    return cases, black


def compare_row(case, reference, row, excel):
    if row.keys() != reference.keys():
        raise ValueError(f"{case} fields differ: {row.keys() ^ reference.keys()}")
    for key, expected in reference.items():
        value = row[key]
        if excel and expected is None and value == "":
            value = None
        if value != expected or isinstance(value, bool) != isinstance(expected, bool):
            raise ValueError(f"{case} {reference['id']} {key}: {value!r} != {expected!r}")


def compare_rows(case, wanted, actual, excel=False):
    if len(actual) != len(wanted):
        raise ValueError(f"{case} row count {len(actual)} != {len(wanted)}")
    for reference, row in zip(wanted, actual):
        compare_row(case, reference, row, excel)


def compare_black(sdk, python, excel):
    keys = ("plus", "minus", "slope", "standard_error", "reference")
    for values in (sdk, python, [excel[k] for k in keys]):
        if values is None or len(values) != len(keys):
            raise ValueError("Missing direct flat-IV result")
        for value, reference, tolerance in zip(values, sdk, (1e-11, 1e-11, 1e-8, 1e-8, 1e-8)):
            if not math.isclose(value, reference, rel_tol=0, abs_tol=tolerance):
                raise ValueError(f"Direct flat-IV result {value} differs from SDK {reference}")
    print("Direct parallel shift: both PVs, slope, paired SE and Black reference matched")


def verify(sdk_path, python_path, excel_path):
    sdk, sdk_black = capture(sdk_path, sdk=True)
    python, python_black = capture(python_path)
    workbook = json.loads(excel_path.read_text(encoding="utf-8"))
    cases = {"full", "half", "all_buckets", "partial"}
    if set(sdk) != cases or set(python) != cases:
        raise ValueError("Incomplete quote fixtures")
    for case in sorted(cases):
        compare_rows(case, sdk[case], python[case])
        compare_rows(case, sdk[case], workbook[case]["rows"], excel=True)
        if workbook[case]["erased_requests"] != 1:
            raise ValueError(f"{case} missing ownership check")
        print(f"{case}: {len(sdk[case])} rows, {len(sdk[case][0])} fields matched")
    compare_black(sdk_black, python_black, workbook["black"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("sdk", "python", "excel"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(args.sdk, args.python, args.excel)

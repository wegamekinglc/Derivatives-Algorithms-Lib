"""Frozen Validation prefixes for three additional common-random-number profiles.

No independence is claimed between products or nested prefixes. Different
dimensions are checked against separate scalar coordinate ledgers, never pooled.
"""

import argparse
import json
from pathlib import Path

from xccy_acceptance_statistics import (
    add_reference_arguments,
    check_reservation,
    coefficient_summary,
    complete_acceptance,
    gradient_summary,
    load_qualified_oracle,
    qualify_reference,
    require,
)

PROFILES = {
    "fixed": ("rho-df/FIXED/foreign/pay", 1, 25),
    "resettable": ("negative-basis/RESETTABLE/foreign/receive", -1, 30),
    "nonflat": ("nonflat/MARK_TO_MARKET/foreign/pay", 1, 40),
}


def frozen_coordinates(profile):
    dates = {"2026-10-21", "2027-04-30", "2027-05-04", "2027-08-02", "2027-08-04", "2027-11-04"}
    if profile != "fixed":
        dates.add("2027-11-08")
    if profile == "nonflat":
        dates.update({"2027-04-19", "2027-10-21"})
    ordered = sorted(dates)
    return [f"interval:{a}:{b}:innovation:{slot}" for a, b in zip(ordered, ordered[1:]) for slot in ("xd", "xf", "Id", "If", "Z")]


def analyze(data, oracle):
    case_id, sign, dimension = PROFILES[data["profile"]]
    check_reservation(data, dimension)
    require(data["phase"] == "Validation", "only Validation in new profile matrix")
    require(data["coordinates"] == frozen_coordinates(data["profile"]), "complete independent interval/slot map")
    reference = next(row for row in oracle["cases"] if row["id"] == case_id)
    qualify_reference(data, reference)
    output, rows, means = coefficient_summary(data, reference, sign)
    output["case_id"] = case_id
    output["dimension"] = dimension
    output["gradients"] = [gradient_summary(data, reference, rows, means, j, label) for j, label in enumerate(data["labels"])]
    output["numerics"] = complete_acceptance(output)
    output["budget_exhausted"] = not output["numerics"]
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oracle", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("captures", type=Path, nargs="+")
    parser.add_argument("--strict", action="store_true")
    add_reference_arguments(parser)
    args = parser.parse_args()
    oracle = load_qualified_oracle(args.oracle, args.reference_capture, args.contexts, args.legacy_records, [p[0] for p in PROFILES.values()])
    reports = [analyze(json.loads(path.read_text()), oracle) for path in args.captures]
    args.output.write_text(json.dumps({"runs": reports}, indent=2) + "\n")
    for report in reports:
        print(report["case_id"], report["factor"], "numerics", report["numerics"])
    if args.strict:
        require(all(report["numerics"] for report in reports), "frozen matrix acceptance failed; estimates retained")


if __name__ == "__main__":
    main()

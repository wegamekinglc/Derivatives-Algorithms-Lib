"""Freeze generated reference fixtures from an explicit completed capture."""

import argparse
import json
from pathlib import Path

from qualify import digest
from run import write_json


def lines(path, rows):
    path.write_text(
        "".join(
            json.dumps(row, allow_nan=False, separators=(",", ":")) + "\n"
            for row in rows
        ),
        encoding="utf-8",
    )


def freeze(evidence, destination):
    request = json.loads((evidence / "request.json").read_text(encoding="utf-8"))
    capture = json.loads((evidence / "capture.json").read_text(encoding="utf-8"))
    report = json.loads((evidence / "qualification.json").read_text(encoding="utf-8"))
    destination.mkdir(parents=True, exist_ok=True)
    lines(
        destination / "quotes.jsonl",
        [
            dict(fixture=row["id"], **quote)
            for row in request["fixtures"]
            for quote in row["quotes"]
        ],
    )
    write_json(
        destination / "snapshots.json",
        [
            dict(fixture=row["id"], snapshot=row["snapshot"])
            for row in capture["fixtures"]
            if row["fit_success"]
        ],
    )
    lines(
        destination / "derivatives.jsonl",
        [
            dict(fixture=fixture["id"], **row)
            for fixture in report["fixtures"]
            for row in fixture["high_precision"]["rows"]
        ],
    )
    lines(destination / "pde.jsonl", report["pde_studies"])
    write_json(
        destination / "manifest.json",
        dict(
            schema="dal.market-iv-independent-fixtures/1",
            decimal_digits=90,
            baseline="53a2b1592993f3a1d354df2d6ec2ae6918e5ef6d",
            capture_sha256=digest(evidence / "capture.json"),
            report_sha256=digest(evidence / "qualification.json"),
            files={
                name: digest(destination / name)
                for name in (
                    "quotes.jsonl",
                    "snapshots.json",
                    "derivatives.jsonl",
                    "pde.jsonl",
                )
            },
            status=report["status"],
        ),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    freeze(args.evidence, args.destination)

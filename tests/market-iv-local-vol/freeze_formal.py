"""Freeze reviewable qualification rows and the source evidence checksums."""

import argparse
import json
from pathlib import Path

from qualify import digest
from run import write_json


def write_lines(path, rows):
    path.write_text(
        "".join(
            json.dumps(row, separators=(",", ":"), allow_nan=False) + "\n"
            for row in rows
        ),
        encoding="utf-8",
    )


def freeze(directory, destination):
    destination.mkdir(parents=True, exist_ok=True)
    report = json.loads((directory / "formal-qualification.json").read_text())
    grids = json.loads((directory / "grid-pde-qualified.json").read_text())
    for row in grids:
        if row["reference_self_check"]:
            row["reference_self_check"].pop("levels")
    write_lines(destination / "formal-rows.jsonl", report["rows"])
    write_lines(destination / "grid-admission.jsonl", grids)
    ledger = json.loads((directory / "sampling-verification.json").read_text())
    write_json(destination / "sampling-ledger.json", ledger)
    names = ("formal-rows.jsonl", "grid-admission.jsonl", "sampling-ledger.json")
    evidence = (
        "mc-request.json",
        "capture.json",
        "grid-pde-qualified.json",
        "formal-qualification.json",
        "F1_knots-formal.json",
        "F1_flat-formal.json",
        "F2_linear-formal.json",
        "F2_nonlinear-formal.json",
    )
    write_json(
        destination / "formal-manifest.json",
        dict(
            schema="dal.market-iv-formal-fixtures/1",
            counts=report["counts"],
            accepted=report["accepted"],
            files={name: digest(destination / name) for name in names},
            evidence_sha256={name: digest(directory / name) for name in evidence},
        ),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    freeze(args.directory, args.destination)

"""Validate the complete v2 generic joint quote-risk oracle, without relaxed gates."""

import argparse
import csv
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import sys


SCHEME = "dal.generic-joint-quote-risk-evidence/1"
EPSILON = sys.float_info.epsilon
TARGETS = {5: 5e-6, 10: 1e-4, 16: 1e-3}
PARAMETERIZATIONS = ("PIECEWISE_CONSTANT_FWD", "PIECEWISE_LINEAR_FWD", "LOG_DISCOUNT", "ZERO_RATE")
STATES = ("base", "plus_h", "minus_h", "plus_b", "minus_b")
FIELDS = (
    "fixture_id", "blocks", "width", "mode", "parameterization", "layered", "currency", "quote_key", "axis_fingerprint",
    "price_scale", "api_derivative", "oracle_derivative", "derivative_abs_error", "derivative_rel_error",
    "derivative_abs_threshold", "derivative_rel_threshold", "api_dv01", "oracle_dv01", "dv01_abs_error", "dv01_rel_error",
    "dv01_abs_threshold", "dv01_rel_threshold", "unit_error", "unit_threshold",
    *(f"pv_{state}" for state in STATES), *(f"gross_{state}" for state in STATES),
)


def expected_buckets():
    """Freeze the declaration/quote manifest independently of the measured CSV."""
    expected = {}
    for blocks, width, mode, parameterization, layered in itertools.product(
        (2, 3), TARGETS, ("ANALYTIC", "BUMPED"), PARAMETERIZATIONS, (0, 1)
    ):
        suffix = "layered" if layered else "unlayered"
        fixture = f"b{blocks}-n{width}-{mode}-{parameterization}-{suffix}"
        for block in range(blocks):
            count = width // blocks + int(block < width % blocks)
            for ordinal in range(count):
                expected[(fixture, f"curve:{block}:{ordinal}")] = (blocks, width, mode, parameterization, layered)
    return expected


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_number(actual, expected, name):
    require(math.isclose(actual, expected, rel_tol=2e-13, abs_tol=1e-25), f"inconsistent {name}: {actual} versus {expected}")


def relative_error(error, first, second):
    denominator = max(abs(first), abs(second))
    return error / denominator if denominator else 0.0


def validate_numerics(row, width):
    numbers = {key: float(row[key]) for key in FIELDS[9:]}
    require(all(math.isfinite(value) for value in numbers.values()), "non-finite numerical observation")
    for state in STATES:
        require(numbers[f"gross_{state}"] >= abs(numbers[f"pv_{state}"]), f"invalid gross PV at {state}")
    scale = max(1.0, *(numbers[f"gross_{state}"] for state in STATES))
    derivative = (numbers["pv_plus_h"] - numbers["pv_minus_h"]) / 2e-6
    dv01 = (numbers["pv_plus_b"] - numbers["pv_minus_b"]) / 2
    verify_number(numbers["price_scale"], scale, "price scale")
    verify_number(numbers["oracle_derivative"], derivative, "oracle derivative")
    verify_number(numbers["oracle_dv01"], dv01, "oracle DV01")
    for name, oracle, multiplier in (("derivative", derivative, 1.0), ("dv01", dv01, 1e-4)):
        actual = numbers[f"api_{name}"]
        error = abs(actual - oracle)
        relative = relative_error(error, actual, oracle)
        absolute_limit = TARGETS[width] * scale * multiplier
        verify_number(numbers[f"{name}_abs_error"], error, f"{name} absolute error")
        verify_number(numbers[f"{name}_rel_error"], relative, f"{name} relative error")
        verify_number(numbers[f"{name}_abs_threshold"], absolute_limit, f"{name} absolute threshold")
        verify_number(numbers[f"{name}_rel_threshold"], TARGETS[width], f"{name} relative threshold")
        require(error <= absolute_limit or relative <= TARGETS[width], f"{name} exceeds both thresholds")
    unit_error = abs(numbers["api_dv01"] - 1e-4 * numbers["api_derivative"])
    unit_limit = 64 * EPSILON * max(scale * 1e-4, abs(numbers["api_dv01"]), 1e-4 * abs(numbers["api_derivative"]))
    verify_number(numbers["unit_error"], unit_error, "unit error")
    verify_number(numbers["unit_threshold"], unit_limit, "unit threshold")
    require(unit_error <= unit_limit, "DV01 unit identity failed")


def validate(rows):
    expected = expected_buckets()
    observed = set()
    fingerprints = {}
    for row in rows:
        require(set(row) == set(FIELDS), "unexpected CSV fields")
        identity = (row["fixture_id"], row["quote_key"])
        require(identity in expected, f"unexpected quote bucket {identity}")
        require(identity not in observed, f"duplicate quote bucket {identity}")
        observed.add(identity)
        metadata = (int(row["blocks"]), int(row["width"]), row["mode"], row["parameterization"], int(row["layered"]))
        require(metadata == expected[identity], f"manifest mismatch for {identity}")
        require(row["currency"] == "USD", f"unexpected currency for {identity}")
        fingerprint = row["axis_fingerprint"]
        require(re.fullmatch(r"sha256:[0-9a-f]{64}", fingerprint) is not None, "invalid axis fingerprint")
        require(fingerprints.setdefault(identity[0], fingerprint) == fingerprint, "axis changed within a fixture")
        try:
            validate_numerics(row, metadata[1])
        except (ValueError, OverflowError) as error:
            raise ValueError(f"{identity}: {error}") from error
    require(observed == set(expected), f"missing {len(set(expected) - observed)} required quote buckets")
    return {"scheme": SCHEME, "fixtures": len(fingerprints), "buckets": len(observed), "status": "passed"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{40}", args.source_sha) is not None, "source SHA must be a full commit ID")
    with args.evidence.open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        require(tuple(reader.fieldnames or ()) == FIELDS, "unexpected CSV header")
        report = validate(reader)
    report.update(source_sha=args.source_sha, evidence_sha256=hashlib.sha256(args.evidence.read_bytes()).hexdigest(),
                  derivative_step=1e-6, dv01_step=1e-4, thresholds=TARGETS)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Validated {report['buckets']} buckets across {report['fixtures']} fixtures")


if __name__ == "__main__":
    main()

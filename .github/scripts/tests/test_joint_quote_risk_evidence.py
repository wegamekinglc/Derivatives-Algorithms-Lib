"""Fail-closed completeness and numerical checks for the new joint domain."""

import copy
import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_joint_quote_risk_evidence.py"
SPEC = importlib.util.spec_from_file_location("joint_quote_risk_evidence", SCRIPT)
EVIDENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EVIDENCE)


def valid_rows():
    rows = []
    for identity, metadata in EVIDENCE.expected_buckets().items():
        fixture, key = identity
        blocks, width, mode, parameterization, layered = metadata
        target = EVIDENCE.TARGETS[width]
        row = dict.fromkeys(EVIDENCE.FIELDS, "0")
        row.update(fixture_id=fixture, quote_key=key, blocks=str(blocks), width=str(width), mode=mode,
                   parameterization=parameterization, layered=str(layered), currency="USD",
                   axis_fingerprint="sha256:" + "a" * 64, price_scale="1",
                   derivative_abs_threshold=str(target), derivative_rel_threshold=str(target),
                   dv01_abs_threshold=str(target * 1e-4), dv01_rel_threshold=str(target),
                   unit_threshold=str(64 * EVIDENCE.EPSILON * 1e-4))
        rows.append(row)
    return rows


class JointQuoteRiskEvidenceTest(unittest.TestCase):
    def test_complete_manifest_passes(self):
        rows = valid_rows()
        self.assertEqual(len(rows), 992)
        self.assertEqual(EVIDENCE.validate(rows)["fixtures"], 96)

    def test_missing_or_duplicate_bucket_fails_even_at_expected_row_count(self):
        rows = valid_rows()
        with self.assertRaises(ValueError):
            EVIDENCE.validate(rows[:-1])
        rows[-1] = rows[0].copy()
        with self.assertRaises(ValueError):
            EVIDENCE.validate(rows)

    def test_extra_bucket_or_wrong_manifest_coordinates_fail(self):
        rows = valid_rows()
        for key, value in (("width", "16"), ("mode", "APPROXIMATE"), ("currency", "EUR"),
                           ("quote_key", "curve:99:0"), ("axis_fingerprint", "unknown")):
            changed = copy.deepcopy(rows)
            changed[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                EVIDENCE.validate(changed)

    def test_nonfinite_values_or_automatic_threshold_relaxation_fail(self):
        rows = valid_rows()
        for key, value in (("oracle_derivative", "nan"), ("api_dv01", "inf"), ("price_scale", "0"),
                           ("derivative_abs_threshold", "100"), ("dv01_rel_threshold", "1")):
            changed = copy.deepcopy(rows)
            changed[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                EVIDENCE.validate(changed)

    def test_raw_shocked_price_and_gross_scale_are_recomputed(self):
        rows = valid_rows()
        for key, value in (("pv_plus_h", "1"), ("gross_minus_b", "10"), ("gross_base", "-1")):
            changed = copy.deepcopy(rows)
            changed[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                EVIDENCE.validate(changed)

    def test_derivative_and_dv01_must_both_pass(self):
        rows = valid_rows()
        rows[0]["api_derivative"] = "1"
        rows[0]["derivative_abs_error"] = "1"
        rows[0]["derivative_rel_error"] = "1"
        rows[0]["api_dv01"] = "0.0001"
        rows[0]["dv01_abs_error"] = "0.0001"
        rows[0]["dv01_rel_error"] = "1"
        with self.assertRaises(ValueError):
            EVIDENCE.validate(rows)

    def test_inconsistent_fingerprint_within_fixture_fails(self):
        rows = valid_rows()
        rows[0]["axis_fingerprint"] = "sha256:" + "b" * 64
        with self.assertRaises(ValueError):
            EVIDENCE.validate(rows)


if __name__ == "__main__":
    unittest.main()

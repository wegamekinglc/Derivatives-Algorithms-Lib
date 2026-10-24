"""Admission regressions for the independent paired-batch acceptance tools."""

import copy
import hashlib
import json
import statistics
import tempfile
import unittest
from pathlib import Path

from xccy_acceptance_statistics import analyze, legacy_fingerprint, load_qualified_oracle

CASE = "flat/MARK_TO_MARKET/domestic/receive"
LABEL = "fxspot:EUR/USD"


def direct(value):
    return {"actual": str(value), "oracle": str(value), "oracle_indicator": "0", "fast_indicator": "0", "error": "0", "passed": True}


def rank_one_covariance(deviations):
    slopes = (2, 1, 3, 2)
    return [[a * b * statistics.variance(deviations) for b in slopes] for a in slopes]


def fixture():
    deviations = [(i - 7.5) * 0.001 for i in range(16)]
    covariance = rank_one_covariance(deviations)
    batches = [{"b": 1 + 2 * t, "a": -10 + t, "db": [2 + 3 * t], "da": [-0.5 + 2 * t]} for t in deviations]
    standard_error = statistics.stdev(deviations) / 4
    ranges = [[262144 + i * 16384, 262144 + (i + 1) * 16384] for i in range(16)]
    data = {
        "source": "economic-source",
        "instrument": "mc-display-identity",
        "binding": "full-binding",
        "measure": "Price",
        "model": {"identity": "model", "spot": 1.1},
        "mapping": "five-slot-map",
        "dimension": 30,
        "phase": "Validation",
        "factor": 1,
        "validation": True,
        "held_out": False,
        "labels": [LABEL],
        "batches": batches,
        "coefficients": {"b": 1.0, "a": -10.0, "db": [2.0], "da": [-0.5]},
        "covariance": covariance,
        "mean": 0.99,
        "se": 2.001 * standard_error,
        "gradient": [1.9995],
        "gradient_se": [3.002 * standard_error],
        "ranges": [[a, b, a * 30, b * 30] for a, b in ranges],
        "consumed": 16 * 16384 * 30,
        "reservation": "Validation-reservation",
        "prepare_seconds": 0.0,
        "evaluate_seconds": 0.0,
    }
    context = {
        "source": data["source"],
        "model": copy.deepcopy(data["model"]),
        "mapping": data["mapping"],
        "dimension": 30,
        "fixture_instrument": "reference-display-identity",
        "instrument_aliases": ["reference-display-identity", data["instrument"]],
        "binding": "full-binding",
        "measure": "Price",
        "selected_labels": [LABEL],
        "native_labels": [LABEL],
        "legacy_records": [],
    }
    reference = {
        "id": CASE,
        "context": context,
        "values": [direct(x) for x in (-10.0, 1.0, 0.99, 0.1)],
        "gradients": [{"label": LABEL, "scale": ".01", "h": ".00001", "values": [direct(x) for x in (-0.5, 2.0, 1.9995, 0.205)]}],
    }
    return data, {"cases": [reference]}


def reference_files(directory):
    data, oracle = fixture()
    context = dict(oracle["cases"][0]["context"], id=CASE)
    captured = {key: context[key] for key in ("id", "source", "model", "mapping", "dimension")}
    captured.update(instrument=context["fixture_instrument"], labels=context["native_labels"])
    capture_path, oracle_path, context_path = [Path(directory) / name for name in ("capture.json", "oracle.json", "contexts.json")]
    capture_path.write_text(json.dumps([captured]))
    oracle["capture_sha256"] = hashlib.sha256(capture_path.read_bytes()).hexdigest()
    oracle_path.write_text(json.dumps(oracle))
    context_path.write_text(json.dumps([context]))
    return data, oracle_path, capture_path, context_path


class XccyAcceptanceStatisticsTest(unittest.TestCase):
    def test_frozen_capture_hash_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            data, oracle, capture, contexts = reference_files(directory)
            self.assertFalse(analyze(data, load_qualified_oracle(oracle, capture, contexts))["numerics"])
            capture.write_text(capture.read_text() + " ")
            with self.assertRaisesRegex(ValueError, "capture SHA256"):
                load_qualified_oracle(oracle, capture, contexts)

    def test_frozen_context_is_checked_against_reference_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            _, oracle, capture, contexts = reference_files(directory)
            wrong = json.loads(contexts.read_text())
            wrong[0]["model"]["spot"] = 1.2
            contexts.write_text(json.dumps(wrong))
            with self.assertRaisesRegex(ValueError, "fixture mismatch: model"):
                load_qualified_oracle(oracle, capture, contexts)

    def test_legacy_missing_metadata_requires_a_prequalified_record(self):
        data, oracle = fixture()
        del data["binding"]
        del data["measure"]
        with self.assertRaisesRegex(ValueError, "legacy.*provenance"):
            analyze(data, oracle)
        oracle["cases"][0]["context"]["legacy_records"] = [legacy_fingerprint(data)]
        self.assertFalse(analyze(data, oracle)["numerics"])
        data["mean"] += 1e-12
        with self.assertRaisesRegex(ValueError, "legacy.*provenance"):
            analyze(data, oracle)

    def test_qualified_context_allows_distinct_display_identities(self):
        data, oracle = fixture()
        result = analyze(data, oracle)
        self.assertEqual(result["mapping"], "five-slot-map")
        self.assertFalse(result["numerics"])

    def test_failed_selected_direct_gradient_is_rejected(self):
        data, oracle = fixture()
        oracle["cases"][0]["gradients"][0]["values"][2]["passed"] = False
        with self.assertRaisesRegex(ValueError, "direct gradient"):
            analyze(data, oracle)

    def test_missing_selected_gradient_is_rejected(self):
        data, oracle = fixture()
        oracle["cases"][0]["gradients"] = []
        with self.assertRaisesRegex(ValueError, "selected gradient"):
            analyze(data, oracle)

    def test_duplicate_gradient_reference_is_rejected(self):
        data, oracle = fixture()
        oracle["cases"][0]["gradients"] *= 2
        with self.assertRaisesRegex(ValueError, "gradient.*unique"):
            analyze(data, oracle)

    def test_invalid_indicators_cannot_borrow_a_passed_flag(self):
        for invalid in ("nan", "inf", "-.1", "1"):
            with self.subTest(indicator=invalid):
                data, oracle = fixture()
                oracle["cases"][0]["gradients"][0]["values"][2]["oracle_indicator"] = invalid
                with self.assertRaisesRegex(ValueError, "direct gradient"):
                    analyze(data, oracle)

    def test_gradient_scales_and_steps_remain_frozen(self):
        for key in ("scale", "h"):
            with self.subTest(field=key):
                data, oracle = fixture()
                oracle["cases"][0]["gradients"][0][key] = ".1"
                with self.assertRaises(ValueError):
                    analyze(data, oracle)

    def test_wrong_context_fields_are_rejected(self):
        for key in ("source", "mapping", "instrument", "binding", "measure"):
            with self.subTest(field=key):
                data, oracle = fixture()
                data[key] = "wrong-" + key
                with self.assertRaisesRegex(ValueError, key):
                    analyze(data, oracle)

    def test_model_contents_are_checked_beyond_identity(self):
        for key, value in (("identity", "wrong-model"), ("spot", 1.2)):
            with self.subTest(field=key):
                data, oracle = fixture()
                data["model"][key] = value
                with self.assertRaisesRegex(ValueError, "model"):
                    analyze(data, oracle)

    def test_missing_context_is_rejected(self):
        data, oracle = fixture()
        del oracle["cases"][0]["context"]
        with self.assertRaisesRegex(ValueError, "reference context"):
            analyze(data, oracle)

    def test_incomplete_covariance_is_rejected(self):
        for shape in ("empty", "missing-row", "short-row"):
            with self.subTest(shape=shape):
                data, oracle = fixture()
                if shape == "empty":
                    data["covariance"] = []
                elif shape == "missing-row":
                    data["covariance"].pop()
                else:
                    data["covariance"][0].pop()
                with self.assertRaisesRegex(ValueError, "covariance"):
                    analyze(data, oracle)

    def test_full_size_covariance_still_requires_every_value(self):
        data, oracle = fixture()
        data["covariance"][2][3] += 1e-5
        with self.assertRaisesRegex(ValueError, "full paired coefficient covariance"):
            analyze(data, oracle)

    def test_batch_gradient_axes_are_complete(self):
        for key in ("db", "da"):
            for values in ([], [0.0, 99.0]):
                with self.subTest(axis=key, values=values):
                    data, oracle = fixture()
                    data["batches"][0][key] = values
                    with self.assertRaisesRegex(ValueError, "batch.*axis"):
                        analyze(data, oracle)

    def test_selected_label_cannot_silently_remove_a_reported_axis(self):
        data, oracle = fixture()
        data["labels"].pop()
        with self.assertRaisesRegex(ValueError, "selected gradient axis"):
            analyze(data, oracle)

    def test_top_level_gradient_axes_are_complete(self):
        for key in ("gradient", "gradient_se"):
            with self.subTest(axis=key):
                data, oracle = fixture()
                data[key].append(99.0)
                with self.assertRaisesRegex(ValueError, "gradient.*axis"):
                    analyze(data, oracle)


if __name__ == "__main__":
    unittest.main()

"""Independent bounds, source-admission and paired-uncertainty controls."""

import importlib.util
import math
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "dal-cpp/benchmarks/script_mc_perf/lsmc_study_support.py"
sys.path.insert(0, str(MODULE.parent))
from native_fixture import native_sources
SPEC = importlib.util.spec_from_file_location("lsmc_study_support", MODULE)
SUPPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUPPORT)
STOCK = native_sources()["dal-cpp/dal/script/lsmc.cpp"]


class SourceAdmissionTest(unittest.TestCase):
    def test_control_replaces_both_addresses_and_adds_range_admission(self):
        patched = SUPPORT.controlled_source(STOCK, 1024)
        self.assertEqual(STOCK.count("return {training, validation, training + validation, replicates};"), 1)
        self.assertNotIn("return {training, validation, training + validation, replicates};", patched)
        self.assertIn("return {training, validation, studyAnchor + validation, replicates};", patched)
        self.assertIn("studyAnchor >= training", patched)
        self.assertIn("MAX_SOBOL_PATHS - studyAnchor - validation", patched)
        self.assertIn("BatchPlan_(counts.validation_, 1), static_cast<size_t>(DAL_LSMC_STUDY_PRICING_OFFSET)", patched)
        self.assertNotIn("DAL_LSMC_STUDY_PRICING_OFFSET", STOCK)

    def test_unknown_function_shapes_and_repeated_application_are_rejected(self):
        mutations = (
            STOCK.replace("const size_t training = simulation", "const size_t training = 1 + simulation", 1),
            STOCK.replace("validationCtx.constValues_ = ctx.constValues_;", "validationCtx.constValues_ = nullptr;", 1),
            STOCK.replace("return {training, validation, training + validation, replicates};", "return {};", 1),
        )
        for source in mutations:
            with self.subTest(source=source[:30]), self.assertRaises(ValueError):
                SUPPORT.controlled_source(source, 1024)
        with self.assertRaises(ValueError):
            SUPPORT.controlled_source(SUPPORT.controlled_source(STOCK, 1024), 1024)

    def test_invalid_anchors_are_rejected(self):
        for anchor in (0, -1, 2**31, True, "1024"):
            with self.subTest(anchor=anchor), self.assertRaises(ValueError):
                SUPPORT.controlled_source(STOCK, anchor)

    def test_current_checkpoint_training_shape_is_rejected(self):
        current = (ROOT / "dal-cpp/dal/script/lsmc.cpp").read_text(encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "unrecognized native source shape: TrainFrozenPolicy"):
            SUPPORT.controlled_source(current, 1024)


class AddressLayoutTest(unittest.TestCase):
    def test_training_validation_and_pricing_ranges_are_disjoint(self):
        layout = SUPPORT.block_layout(1024, 256, 64, 512, 2)
        self.assertEqual(layout["training"], (0, 256))
        self.assertEqual(layout["validation"], (1024, 1088))
        self.assertEqual(layout["pricing"], ((1088, 1600), (1600, 2112)))

    def test_unset_budget_and_the_last_admissible_point_follow_native_rules(self):
        default = SUPPORT.block_layout(1024, None, 0, 257, 2)
        self.assertEqual(default["training"], (0, 257))
        boundary = SUPPORT.block_layout(1, 1, 0, (2**32 - 2) // 2, 2)
        self.assertEqual(boundary["pricing"][-1][1], 2**32 - 1)

    def test_overlap_overflow_and_invalid_counts_are_rejected(self):
        invalid = (
            (128, 129, 0, 32, 2),
            (128, 0, 0, 32, 2),
            (128, 64, -1, 32, 2),
            (128, 64, 0, 0, 2),
            (128, 64, 0, 32, 1),
            (128, 64, 0, 2**31 - 1, 2),
        )
        for counts in invalid:
            with self.subTest(counts=counts), self.assertRaises(ValueError):
                SUPPORT.block_layout(*counts)


class UncertaintyTest(unittest.TestCase):
    def test_paired_difference_precedes_the_error_estimate(self):
        reference = (10.0, 20.0, 30.0, 40.0)
        lower = (8.0, 19.0, 31.0, 42.0)
        result = SUPPORT.paired_difference(lower, reference)
        self.assertEqual(result["difference"], 0.0)
        self.assertAlmostEqual(result["pricing_se"], math.sqrt(10.0 / 12.0), delta=1e-14)
        self.assertEqual(result["reference_mean"], 25.0)

    def test_zero_variance_and_zero_reference_are_reported_without_infinity(self):
        constant = SUPPORT.paired_difference((3.0, 3.0), (2.0, 2.0))
        self.assertEqual(constant["difference"], 1.0)
        self.assertEqual(constant["pricing_se"], 0.0)
        zero = SUPPORT.paired_difference((1.0, -1.0), (0.0, 0.0))
        self.assertIsNone(zero["relative_difference"])
        self.assertEqual(zero["scaled_difference"], 0.0)

    def test_missing_nonfinite_or_single_observations_are_rejected(self):
        for left, right in (((1.0,), (1.0,)), ((1.0, 2.0), (1.0,)), ((math.nan, 1.0), (0.0, 0.0))):
            with self.subTest(left=left, right=right), self.assertRaises(ValueError):
                SUPPORT.paired_difference(left, right)


if __name__ == "__main__":
    unittest.main(verbosity=2)

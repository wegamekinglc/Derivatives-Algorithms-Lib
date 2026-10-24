"""Regression tests for full paired-batch basket admission."""

import copy
import statistics
import unittest

from xccy_acceptance_basket import check_covariances, normal_trace


def rank_one_covariance(slopes):
    return [[left * right * (20 / 3) * 1e-10 for right in slopes] for left in slopes]


def fixture():
    means = [1., -10., 2., -.5] * 3
    slopes = list(range(1, 13))
    rows = [[mean + slope * d * 1e-5 for mean, slope in zip(means, slopes)] for d in (-3, -1, 1, 3)]
    covariance = rank_one_covariance(slopes)
    members = []
    for i in range(3):
        members.append({"estimate": {"b": 1., "a": -10., "db": [2.], "da": [-.5], "gradient": [1.9995], "gradient_se": [1.],
                                      "covariance": [row[i * 4:(i + 1) * 4] for row in covariance[i * 4:(i + 1) * 4]]}})
    return {"labels": ["axis"], "means": means, "C_batch": covariance,
            "C_mean": [[x / 4 for x in row] for row in covariance], "members": members}, rows


class BasketAdmissionTest(unittest.TestCase):
    def test_defined_tail_and_ideal_gaussian_accuracy_remain_separate(self):
        actual = normal_trace(1024, 339476552, 1)[0]
        self.assertEqual(actual, 4.224211942570607)
        ideal = statistics.NormalDist().inv_cdf(.9999880110839163)
        self.assertGreater(abs(actual - ideal), 5e-13)
        self.assertNotEqual(actual, normal_trace(1024, 339476553, 1)[0])

    def test_complete_cross_member_covariance_uses_B_rows(self):
        data, rows = fixture()
        means, covariance = check_covariances(data, rows, 4)
        self.assertEqual(len(means), 12)
        self.assertGreater(covariance[0][11], 0)

    def test_cross_blocks_and_B_times_members_mutants_are_rejected(self):
        original, rows = fixture()
        for name in ("cross_blocks", "B_times_members"):
            with self.subTest(name=name):
                data = copy.deepcopy(original)
                if name == "cross_blocks":
                    data["C_batch"][0][4] = 0.
                else:
                    data["C_mean"] = [[x / 3 for x in row] for row in data["C_mean"]]
                with self.assertRaises(ValueError):
                    check_covariances(data, rows, 4)

    def test_truncated_matrices_and_summary_axes_are_rejected(self):
        original, rows = fixture()
        for name in ("missing_row", "short_row", "extra_da", "missing_da"):
            with self.subTest(name=name):
                data = copy.deepcopy(original)
                if name == "missing_row":
                    data["C_batch"].pop()
                elif name == "short_row":
                    data["C_batch"][-1].pop()
                elif name == "extra_da":
                    data["members"][-1]["estimate"]["da"].append(0.)
                else:
                    data["members"][-1]["estimate"]["da"].clear()
                with self.assertRaises(ValueError):
                    check_covariances(data, rows, 4)


if __name__ == "__main__":
    unittest.main()

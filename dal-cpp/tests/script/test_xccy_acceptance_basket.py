"""Regression tests for full paired-batch basket admission."""

import copy
import statistics
import unittest

from xccy_acceptance_basket import DIMS, OFFSETS, PHASES, check_covariances, fingerprint, normal_trace, qualified_fit, qualify_member
from xccy_acceptance_extended import independent_schedule


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


def context_fixture():
    members = [{key: key + str(i) for key in ("id", "source", "binding", "context", "mapping", "instrument", "measure")}
               for i in range(3)]
    data = {"mode": "EXACT", "allocation": {"members": copy.deepcopy(members)}, "model_archive": {},
            "result": {"model": "model", "allocation": {"members": copy.deepcopy(members)}}}
    oracle = {"fits": [{"mode": "EXACT", "cases": [{}, {}, {}],
                        "context": {"model": "model", "archive": fingerprint({}), "members": members}}]}
    return data, oracle


def native_fixture():
    curves = [{"currency": currency, "route": route, "dates": ["2026-10-21", "2027-11-08"],
               "labels": ["logdf:" + currency + ":" + route + ":2027-11-08"], "jacobian": [[0.], [slope]]}
              for currency, route, slope in (("USD", "OIS", .5), ("EUR", "OIS", 1.), ("USD", "BASIS:EUR/USD", 1.))]
    labels = ["fxspot:EUR/USD"] + [c["labels"][0] for c in curves]
    labels += ["sigma:" + name + ":2027-11-08" for name in ("USD", "EUR", "EUR/USD")]
    slopes = [1., .5, 1., 1., 1., 1., 1.]
    chain = [[slopes[i] if i == j else 0. for j in range(7)] for i in range(7)]
    return {"curves": curves, "bucket_dates": ["2027-11-08"]}, labels, chain


def direct_fixture():
    model, labels, chain = native_fixture()
    case = {"model": model, "labels": labels,
            "fast": [1., 2., 3., 4.], "fast_gradient": [[float(i + j) for j in range(7)] for i in range(4)],
            "source": "source", "instrument": "instrument", "mapping": "mapping", "grid": [], "coordinates": []}
    case["domestic_periods"], case["foreign_periods"] = independent_schedule(case)
    member = {key: case[key] for key in ("source", "instrument", "mapping", "grid", "coordinates")}
    member.update(D=DIMS[0], block=list(OFFSETS[:2]), id="independent-staged-0", measure="Price",
                  phases=[[begin, end, begin * DIMS[0], end * DIMS[0]] for begin, end in PHASES])
    direct = {key: copy.deepcopy(case[key]) for key in ("labels", "fast", "fast_gradient")}
    direct["native_chain"] = chain
    return member, case, direct


class BasketAdmissionTest(unittest.TestCase):
    def test_complete_context_and_native_mapping_are_admitted(self):
        data, oracle = context_fixture()
        qualified_fit(data, oracle)
        qualify_member(0, *direct_fixture())

    def test_missing_reference_context_cannot_hide_synchronously_wrong_member_fields(self):
        for key in ("id", "source", "binding", "context", "mapping", "instrument", "measure"):
            with self.subTest(missing=key):
                data, oracle = context_fixture()
                del oracle["fits"][0]["context"]["members"][0][key]
                data["allocation"]["members"][0][key] = "wrong-" + key
                data["result"]["allocation"] = copy.deepcopy(data["allocation"])
                with self.assertRaisesRegex(ValueError, "complete oracle member context"):
                    qualified_fit(data, oracle)

    def test_incomplete_or_nonfinite_direct_derivatives_are_rejected(self):
        malformed = ([], [[0.] * 7] * 3, [[0.] * 7] * 5, [[0.] * 6] * 4, [[0.] * 8] * 4,
                     [[float("nan")] * 7] * 4, [[float("inf")] * 7] * 4)
        for matrix in malformed:
            with self.subTest(matrix=matrix):
                member, case, direct = direct_fixture()
                direct["fast_gradient"] = matrix
                with self.assertRaisesRegex(ValueError, "complete direct A/B/Price/par derivatives"):
                    qualify_member(0, member, case, direct)

    def test_incomplete_nonfinite_or_wrong_coordinate_native_chain_is_rejected(self):
        _, _, original = direct_fixture()
        chain = original["native_chain"]
        malformed = ([], chain[:-1], [row[:-1] for row in chain], [[float("nan")] * 7] * 7,
                     [[float("inf")] * 7] * 7, list(reversed(chain)), [list(reversed(row)) for row in chain])
        for matrix in malformed:
            with self.subTest(matrix=matrix):
                member, case, direct = direct_fixture()
                direct["native_chain"] = matrix
                with self.assertRaisesRegex(ValueError, "native chain"):
                    qualify_member(0, member, case, direct)

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

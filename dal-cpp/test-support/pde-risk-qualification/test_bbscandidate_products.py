import json
import pathlib
import unittest
from unittest.mock import patch

import bbscandidate_products as products
from fixtures import fixtures

ROOT = pathlib.Path(__file__).resolve().parents[3]
CANDIDATE = ROOT / ".codex/artifacts/validation/dal502-bbs-candidate"
FIXTURE = ROOT / ".codex/artifacts/validation/dal502-candidate-p2/test-cases.json"
PREVIOUS = (
    ROOT / ".codex/artifacts/validation/dal502-american-directions/directions.json"
)


def replay_case(name, direction, change=None, reference_change=None):
    saved = json.loads((CANDIDATE / "products-candidate.json").read_text())
    cases = saved["cases"]
    raw = json.loads(FIXTURE.read_text())[name]["raw"]
    proof = json.loads((CANDIDATE / "result-qualified.json").read_text())["proofs"][
        cases[name]["binding"]
    ]
    if change:
        change(raw)
    if reference_change:
        reference_change(proof)
    with patch.object(products, "read_capture", return_value=raw):
        cases[name] = products.product_case(
            name, fixtures()[name], cases[name]["sources"], proof
        )
    previous = json.loads(PREVIOUS.read_text())
    directions = saved["directions"]
    directions[direction] = products.direction_case(previous[direction], cases)
    groups = products.group_status(cases, saved["premiums"], directions)
    return dict(case=cases[name], direction=directions[direction], groups=groups)


def scenario_value(field, value):
    return lambda raw: raw["scenarios"][0].__setitem__(field, value)


def unavailable_vega(raw):
    raw["risks"][2]["status_"] = "UNAVAILABLE"


def capacity_failure(field):
    limits = dict(
        workUnits_=("original", "risk_", "maxWorkUnits_"),
        numericPayloadBytes_=("plan", "plannedNumericPayloadBytes_"),
        peakWorkspaceBytes_=("plan", "plannedWorkspaceBytes_"),
        peakNativeBytes_=("plan", "plannedPeakNativeBytes_"),
    )

    def change(raw):
        limit = raw
        for key in limits[field]:
            limit = limit[key]
        raw["execution"][field] = limit + 1

    return change


class BbsCandidateProductsTest(unittest.TestCase):
    def assert_failure_reaches_group(self, name, direction, change):
        result = replay_case(name, direction, change)
        self.assertEqual(result["case"]["candidate_error"]["status"], "FAIL")
        self.assertEqual(result["direction"]["finest_comparison"]["status"], "PASS")
        self.assertEqual(result["direction"]["direction_status"], "PASS")
        self.assertEqual(result["direction"]["candidate_status"], "FAIL")
        self.assertEqual(result["groups"][direction.split("-")[0]], "FAIL")
        return result

    def test_original_penalty_linear_failure_blocks_direction_and_n7(self):
        name = "N7-1-penalty-10000000.0"
        result = self.assert_failure_reaches_group(
            name, "N7-1-penalty", scenario_value("maxLinearBackwardError_", 1e-8)
        )
        self.assertEqual(
            result["case"]["hard_checks"][name + "-residual-Base"]["status"], "FAIL"
        )

    def test_middle_original_solver_residuals_block_n6(self):
        fields = [
            "maxObstacleViolation_",
            "maxDualViolation_",
            "maxComplementarityResidual_",
            "maxIterationChange_",
        ]
        for field in fields:
            with self.subTest(field=field):
                self.assert_failure_reaches_group(
                    "N6-1-time-2000", "N6-1-time", scenario_value(field, 1.0)
                )

    def test_middle_mandatory_smooth_failure_blocks_both_groups(self):
        for name, direction in [
            ("N6-1-time-2000", "N6-1-time"),
            ("N7-2-domain-600", "N7-2-domain"),
        ]:
            with self.subTest(direction=direction):
                result = self.assert_failure_reaches_group(
                    name, direction, unavailable_vega
                )
                self.assertEqual(result["case"]["mandatory_smooth"], "FAIL")

    def test_middle_capacities_block_both_groups(self):
        for name, direction in [
            ("N6-1-time-2000", "N6-1-time"),
            ("N7-2-domain-600", "N7-2-domain"),
        ]:
            for field in [
                "workUnits_",
                "numericPayloadBytes_",
                "peakWorkspaceBytes_",
                "peakNativeBytes_",
            ]:
                with self.subTest(direction=direction, field=field):
                    result = self.assert_failure_reaches_group(
                        name, direction, capacity_failure(field)
                    )
                    self.assertEqual(
                        result["case"]["hard_checks"][name + "-capacity"]["status"],
                        "FAIL",
                    )

    def test_middle_solver_status_failure_blocks_n6(self):
        result = self.assert_failure_reaches_group(
            "N6-1-time-2000", "N6-1-time", scenario_value("solverStatus_", "FAILED")
        )
        self.assertEqual(result["case"]["mandatory_smooth"], "FAIL")

    def test_nonfinest_reference_accuracy_does_not_tighten_contract(self):
        def inaccurate(proof):
            proof["normalized"][0] += 0.01

        for change, expected in [
            (inaccurate, "FAIL"),
            (lambda proof: proof.update(status="INCOMPLETE"), "INCOMPLETE"),
        ]:
            with self.subTest(expected=expected):
                result = replay_case(
                    "N6-1-space-401", "N6-1-space", reference_change=change
                )
                self.assertEqual(result["case"]["candidate_error"]["status"], expected)
                self.assertEqual(result["case"]["mandatory_smooth"], "PASS")
                self.assertTrue(
                    all(
                        r["status"] == "PASS"
                        for r in result["case"]["hard_checks"].values()
                    )
                )
                self.assertEqual(result["direction"]["candidate_status"], "PASS")
                self.assertEqual(result["groups"]["N6"], "PASS")

    def test_group_rechecks_required_predicates_even_with_cached_direction_status(self):
        name = "N7-1-penalty-10000000.0"
        result = replay_case(
            name, "N7-1-penalty", scenario_value("maxLinearBackwardError_", 1e-8)
        )
        saved = json.loads((CANDIDATE / "products-candidate.json").read_text())
        saved["cases"][name] = result["case"]
        groups = products.group_status(
            saved["cases"], saved["premiums"], saved["directions"]
        )
        self.assertEqual(groups["N7"], "FAIL")

    def test_exact_projected_real_cases_match_accepted_predicates(self):
        directions = {
            "N7-1-penalty-10000000.0": "N7-1-penalty",
            "N6-1-time-2000": "N6-1-time",
            "N7-2-domain-600": "N7-2-domain",
            "N6-1-space-401": "N6-1-space",
        }
        saved = json.loads((CANDIDATE / "products-candidate.json").read_text())
        for name, direction in directions.items():
            with self.subTest(name=name):
                result = replay_case(name, direction)
                self.assertEqual(result["case"], saved["cases"][name])
                self.assertEqual(result["direction"]["candidate_status"], "PASS")
                self.assertEqual(result["groups"][direction.split("-")[0]], "PASS")


if __name__ == "__main__":
    unittest.main()

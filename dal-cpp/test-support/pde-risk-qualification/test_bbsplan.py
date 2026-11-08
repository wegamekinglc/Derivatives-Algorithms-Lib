import copy
import json
import pathlib
import tempfile
import unittest

from bbsplan import BS_CAP, METHOD, NODE_CAP, cache_key, reserve, specifications
from bbsverify import price, self_proof, check_witness_receipt


class BbsPilotTest(unittest.TestCase):
    def test_missing_or_incomplete_witness_receipt_cannot_admit(self):
        with tempfile.TemporaryDirectory(prefix="bbs-receipt-", dir=".") as temporary:
            directory = pathlib.Path(temporary)
            ledger = dict(jobs={"required": dict(spec=dict(kind="formula"))})
            with self.assertRaises(RuntimeError):
                check_witness_receipt(directory, ledger)
            (directory / "witnesses.json").write_text(
                json.dumps(dict(status="PASS", results={}))
            )
            with self.assertRaises(RuntimeError):
                check_witness_receipt(directory, ledger)

    def test_fixed_plan_accounts_for_all_native_witnesses(self):
        ledger = dict(charged_nodes=0, reserved_bs=100)
        specs = specifications()
        for spec in specs:
            reserve(ledger, spec)
        self.assertEqual(len(specs), 113)
        self.assertEqual(ledger["charged_nodes"], 5638979836)
        self.assertEqual(ledger["reserved_bs"], 917672)

    def test_resource_limits_reject_an_extra_miss(self):
        spec = specifications()[0]
        for field, cap in [("charged_nodes", NODE_CAP), ("reserved_bs", BS_CAP)]:
            ledger = dict(charged_nodes=0, reserved_bs=0)
            ledger[field] = cap
            before = copy.deepcopy(ledger)
            with self.assertRaises(RuntimeError):
                reserve(ledger, spec)
            self.assertEqual(ledger, before)

    def test_method_and_executable_cannot_share_price_identity(self):
        spec = specifications()[0]
        self.assertNotEqual(
            cache_key(spec, dict(executable="a")), cache_key(spec, dict(executable="b"))
        )
        old = dict(spec, method="TREE")
        self.assertNotEqual(cache_key(spec, {}), cache_key(old, {}))
        self.assertEqual(spec["method"], METHOD)

    def test_failed_or_nonfinite_price_is_not_a_reference(self):
        for row in [
            dict(error="deadline"),
            {},
            dict(price=None),
            dict(price=float("nan")),
        ]:
            with self.assertRaises(RuntimeError):
                price(row)

    def test_missing_metric_does_not_pass_quarter_gate(self):
        member = [[0.06, -0.4] for _ in range(3)]
        level = dict(members=[member, member], mean=member)
        self.assertEqual(self_proof([level, level])["status"], "INCOMPLETE")

    def test_coarse_mesh_and_raw_parity_each_prevent_admission(self):
        member = [[0.06, -0.4, 2.3] for _ in range(3)]
        last = dict(
            members=[copy.deepcopy(member), copy.deepcopy(member)],
            mean=copy.deepcopy(member),
        )
        old = copy.deepcopy(last)
        old["mean"][0][2] += 0.006
        self.assertEqual(self_proof([old, last])["status"], "INCOMPLETE")
        parity = copy.deepcopy(last)
        parity["members"][1][0][2] += 0.006
        parity["mean"][0][2] += 0.003
        self.assertEqual(self_proof([last, parity])["status"], "INCOMPLETE")


if __name__ == "__main__":
    unittest.main()

import unittest

from batchplan import NODE_CAP, enqueue
from campaign import read_price
from fixtures import BASE
from referenceplan import (
    binding,
    node_charge,
    price_key,
    price_spec,
    self_qualification,
)


class ReferencePlanTest(unittest.TestCase):
    def test_actual_bumps_bind_reference_identity(self):
        base = dict(BASE, nodes=1601)
        self.assertNotEqual(binding(base), binding(dict(base, nodes=801)))
        self.assertNotEqual(binding(base), binding(dict(base, vol_bump=0.0005)))
        self.assertNotEqual(binding(base), binding(dict(base, carry_bump=0.00005)))
        self.assertEqual(binding(base), binding(dict(base, nodes=801, h=0.5)))

    def test_parity_failure_cannot_be_hidden_by_pair_mean(self):
        even = [[0.1, 0.5, 2.0, 0.3, 0.4, -0.5] for _ in range(3)]
        odd = [[0.1, 0.5, 2.006, 0.3, 0.4, -0.5] for _ in range(3)]
        level = dict(
            members=[even, odd],
            mean=[[0.1, 0.5, 2.003, 0.3, 0.4, -0.5] for _ in range(3)],
        )
        result = self_qualification([level, level], "tree")
        self.assertEqual(result["status"], "INCOMPLETE")

    def test_exact_node_cap_and_duplicate_miss(self):
        spec = price_spec(BASE, "base", 0.0, "tree", 4096)
        ledger = dict(
            identity="binary-a",
            jobs={},
            bridges={},
            cache={},
            deadline_epoch=1e20,
            charged_nodes=NODE_CAP - node_charge(spec),
        )
        self.assertIsNotNone(enqueue(ledger, spec))
        self.assertEqual(ledger["charged_nodes"], NODE_CAP)
        self.assertIsNone(enqueue(ledger, spec))
        with self.assertRaises(RuntimeError):
            enqueue(ledger, price_spec(BASE, "spot", 0.5, "tree", 4096))

    def test_other_execution_identity_needs_explicit_bridge(self):
        spec = price_spec(BASE, "base", 0.0, "tree", 4096)
        old_key = price_key(spec, "binary-a")
        ledger = dict(identity="binary-b", cache={old_key: dict(price=7.0)}, bridges={})
        with self.assertRaises(RuntimeError):
            read_price(ledger, spec)
        ledger["bridges"][price_key(spec, "binary-b")] = old_key
        self.assertEqual(read_price(ledger, spec), 7.0)

    def test_failed_and_nonfinite_nodes_never_supply_a_reference(self):
        spec = price_spec(BASE, "base", 0.0, "tree", 4096)
        key = price_key(spec, "binary-a")
        ledger = dict(
            identity="binary-a", cache={key: dict(error="timeout")}, bridges={}
        )
        with self.assertRaises(RuntimeError):
            read_price(ledger, spec)
        ledger["cache"][key] = dict(price=None)
        with self.assertRaises(RuntimeError):
            read_price(ledger, spec)
        ledger["cache"][key] = dict(price=float("nan"))
        with self.assertRaises(RuntimeError):
            read_price(ledger, spec)

    def test_coarse_mesh_failure_cannot_hide_behind_extra_half(self):
        values = [[0.1, 0.5, 2.0, 0.3, 0.4, -0.5] for _ in range(3)]
        previous = [row[:] for row in values]
        previous[0][3] += 0.0008
        old = dict(members=[previous, previous], mean=previous)
        new = dict(members=[values, values], mean=values)
        self.assertEqual(self_qualification([old, new], "tree")["status"], "INCOMPLETE")


if __name__ == "__main__":
    unittest.main()

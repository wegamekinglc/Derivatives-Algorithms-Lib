import copy
import unittest

from bbscandidate import (
    budget,
    full_gate,
    make_plan,
    qualify_levels,
    reuse_value,
    shard_jobs,
)
from bbsplan import METHOD, cache_key
from referenceplan import fingerprint


class BbsCandidateTest(unittest.TestCase):
    def test_missing_vega_or_nonfinite_component_cannot_pass(self):
        for vector in [[0.0] * 5, [0.0] * 7, [0.0] * 3 + [float("nan")] * 3]:
            self.assertFalse(full_gate(vector))
        self.assertTrue(full_gate([0.0] * 6))
        self.assertFalse(full_gate([0.0, 0.0, 0.0, 0.00075, 0.0, 0.0]))

    def test_reuse_rejects_changed_payload_identity_and_offset(self):
        request = dict(spot=100.5, tree_steps=4096)
        spec = dict(
            method=METHOD, kind="pilot", name="PUT", offset=0.5, request=request
        )
        output = dict(result=dict(price=1.0), method=METHOD)
        key = cache_key(spec, dict(executable="accepted"))
        output["id"] = key
        job = dict(spec=spec, state="COMPLETE", output=output)
        sha = fingerprint(output)
        self.assertEqual(
            reuse_value(key, job, output, sha, request, dict(executable="accepted")),
            1.0,
        )
        for changed in [
            dict(output, result=dict(price=2.0)),
            dict(output, method="TREE"),
        ]:
            with self.assertRaises(RuntimeError):
                reuse_value(
                    key, job, changed, sha, request, dict(executable="accepted")
                )
        for identity, value in [
            (dict(executable="old"), request),
            (dict(executable="accepted"), dict(request, spot=100.25)),
        ]:
            with self.assertRaises(RuntimeError):
                reuse_value(key, job, output, sha, value, identity)
        bad = copy.deepcopy(job)
        bad["spec"]["offset"] = 0.25
        with self.assertRaises(RuntimeError):
            reuse_value(key, bad, output, sha, request, dict(executable="accepted"))

    def test_cumulative_consumption_cannot_be_reset(self):
        budget(41587475848, 6766946)
        for nodes, bs in [(42000000001, 0), (0, 7000001)]:
            with self.assertRaises(RuntimeError):
                budget(nodes, bs)

    def test_coarse_vega_mesh_and_each_raw_parity_are_mandatory(self):
        vector = [0.06, -0.4, 2.3, 0.37, -0.3, 0.27]
        means = [copy.deepcopy(vector) for _ in range(3)]
        level = dict(mean=means, members=[copy.deepcopy(means), copy.deepcopy(means)])
        self.assertEqual(qualify_levels([level, level])["status"], "PASS")
        previous = copy.deepcopy(level)
        previous["mean"][0][3] += 0.0008
        self.assertEqual(qualify_levels([previous, level])["status"], "INCOMPLETE")
        final = copy.deepcopy(level)
        final["members"][1][1][4] += 0.0008
        self.assertEqual(qualify_levels([level, final])["status"], "INCOMPLETE")
        truncated = dict(
            mean=[v[:3] for v in means],
            members=[[v[:3] for v in means] for _ in range(2)],
        )
        self.assertEqual(qualify_levels([truncated, truncated])["status"], "INCOMPLETE")

    def test_exact_union_and_native_batch_limits(self):
        plan = make_plan(dict(executable="test"))
        self.assertEqual(len(plan["bindings"]), 12)
        self.assertEqual(len(plan["jobs"]), 792)
        shards = shard_jobs(plan["jobs"])
        self.assertEqual(sum(len(s) for s in shards), 792)
        for shard in shards:
            self.assertLessEqual(sum(j["charged_nodes"] for j in shard), 6000000000)
            self.assertLessEqual(sum(j["reserved_bs"] for j in shard), 2000000)


if __name__ == "__main__":
    unittest.main()

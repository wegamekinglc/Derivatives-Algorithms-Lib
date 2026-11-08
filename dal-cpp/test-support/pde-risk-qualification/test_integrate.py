import contextlib
import gzip
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

import audit
import integrate
import qualify
from fixtures import BASE
from referenceplan import binding


class IntegrationViewTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="dal502-view-", dir=".")
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.old = self.root / "old"
        self.current = self.root / "current"
        self.view = self.root / "audit-view"
        self.old.mkdir()
        self.current.mkdir()
        self.requests = {
            f"N6-1-space-{n}": dict(BASE, nodes=n) for n in [401, 801, 1601]
        }
        self.names = list(self.requests)
        for name in self.names:
            self.capture(self.old, name, 10.0)
        for name in ["supplement.json", "n9a-audit.json"]:
            (self.current / name).write_text("{}")
        self.references = self.root / "references.json"
        qualify.write_json(
            self.references,
            {
                binding(q): dict(status="PASS", normalized=[0.1])
                for q in self.requests.values()
            },
        )

    def capture(self, directory, name, price):
        with gzip.open(directory / (name + ".json.gz"), "wt") as stream:
            json.dump(
                dict(
                    price=price,
                    status="COMPLETE",
                    risks=[],
                    scenarios=[],
                    execution={},
                    plan=dict(planId_=1, plannedSteps_=1),
                ),
                stream,
            )
        qualify.write_json(directory / (name + ".input.json"), self.requests[name])

    def current_prices(self, final_price):
        for name in self.names:
            self.capture(
                self.current, name, final_price if name == self.names[-1] else 10.0
            )

    def summarize(self, directory):
        outputs = {name: qualify.load(directory, name) for name in self.names}
        checks = {"supplement": dict(status="PASS")}

        def check(name, conditions, evidence):
            checks[name] = dict(
                status="PASS" if all(conditions) else "FAIL", evidence=evidence
            )

        audit.sequence(check, outputs, "N6-1-space", self.names)
        qualify.write_json(directory / "audits.json", checks)

    def analyze(self):
        args = [
            "integrate.py",
            "analyze",
            "--directory",
            str(self.current),
            "--old",
            str(self.old),
            "--references",
            str(self.references),
        ]
        with patch.object(
            integrate, "fixtures", return_value=self.requests
        ), patch.object(qualify, "fixtures", return_value=self.requests), patch.object(
            integrate, "validate_direct_evidence"
        ), patch.object(
            audit, "summarize", side_effect=self.summarize
        ), patch.object(
            sys, "argv", args
        ), contextlib.redirect_stdout(
            io.StringIO()
        ):
            integrate.main()
        return json.loads((self.current / "audits-admitted.json").read_text())[
            "N6-1-space"
        ]

    def prepare(self):
        with patch.object(integrate, "fixtures", return_value=self.requests):
            return integrate.prepare_view(self.old, self.current, self.view)

    def test_historical_pass_becomes_current_fail_on_every_analyze(self):
        historical = self.analyze()
        self.assertEqual(historical["status"], "INCOMPLETE")
        self.assertEqual(historical["historical_status"], "PASS")
        self.current_prices(20.0)
        for _ in range(2):
            current = self.analyze()
            self.assertEqual(current["status"], "FAIL")
            self.assertEqual(current["historical_cases"], [])
            self.assertEqual(current["evidence"]["values"][-1][0], 0.2)
            for name in self.names:
                self.assert_read_source(current, name, self.current, "current")

    def assert_read_source(self, audit_row, name, directory, origin):
        source = audit_row["required_sources"][name]
        self.assertEqual(source["origin"], origin)
        for suffix in [".json.gz", ".input.json"]:
            actual = directory / (name + suffix)
            self.assertEqual(source["files"][suffix]["path"], str(actual.resolve()))
            self.assertEqual(source["files"][suffix]["sha256"], qualify.digest(actual))
            self.assertEqual((self.view / (name + suffix)).resolve(), actual.resolve())

    def test_missing_current_output_reverts_to_historical_incomplete(self):
        self.current_prices(10.0)
        self.assertEqual(self.analyze()["status"], "PASS")
        name = self.names[-1]
        (self.current / (name + ".json.gz")).unlink()
        current = self.analyze()
        self.assertEqual(current["status"], "INCOMPLETE")
        self.assertEqual(current["historical_status"], "PASS")
        self.assertEqual(current["historical_cases"], [name])
        self.assert_read_source(current, name, self.old, "historical")
        self.assertEqual(self.analyze()["status"], "INCOMPLETE")

    def test_current_capture_requires_its_input(self):
        self.current_prices(20.0)
        (self.current / (self.names[-1] + ".input.json")).unlink()
        with self.assertRaises(RuntimeError):
            self.prepare()

    def test_historical_fallback_requires_a_complete_pair(self):
        (self.old / (self.names[-1] + ".input.json")).unlink()
        with self.assertRaises(RuntimeError):
            self.prepare()

    def test_retargeted_view_link_cannot_be_admitted(self):
        self.current_prices(20.0)
        origins = self.prepare()
        target = self.view / (self.names[-1] + ".json.gz")
        target.unlink()
        target.symlink_to((self.old / target.name).resolve())
        with patch.object(
            audit, "summarize", side_effect=self.summarize
        ), self.assertRaises(RuntimeError):
            integrate.admit_audits(self.view, self.current, origins)

    def test_changed_payload_hash_cannot_be_admitted(self):
        self.current_prices(20.0)
        origins = self.prepare()
        self.capture(self.current, self.names[-1], 10.0)
        with patch.object(
            audit, "summarize", side_effect=self.summarize
        ), self.assertRaises(RuntimeError):
            integrate.admit_audits(self.view, self.current, origins)

    def test_unmanaged_view_file_is_preserved_and_rejected(self):
        self.view.mkdir()
        target = self.view / (self.names[0] + ".json.gz")
        target.write_text("unmanaged data")
        with self.assertRaises(RuntimeError):
            self.prepare()
        self.assertEqual(target.read_text(), "unmanaged data")

    def test_auxiliary_view_link_cannot_overwrite_its_old_source(self):
        self.view.mkdir()
        original = self.old / "supplement.json"
        original.write_text("original audit")
        (self.view / "supplement.json").symlink_to(original.resolve())
        self.prepare()
        self.assertEqual(original.read_text(), "original audit")
        self.assertEqual((self.view / "supplement.json").read_text(), "{}")


if __name__ == "__main__":
    unittest.main()

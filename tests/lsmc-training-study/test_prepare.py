"""Preparation reads committed inputs without changing a user's checkout."""

from pathlib import Path
import subprocess  # nosec B404: fixed Git commands in an owned temporary fixture
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
import lsmc_study_prepare as study  # noqa: E402
from lsmc_study_build import program  # noqa: E402


class PreparationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / "source"
        self.source.mkdir()
        self.git("init", "-q")
        self.benchmark = self.source / "dal-cpp/benchmarks/script_mc_perf"
        self.benchmark.mkdir(parents=True)
        (self.benchmark / "lsmctrainingstudy.cpp").write_text("committed sample\n")
        self.git("add", ".")
        self.git("-c", "user.name=Study test", "-c", "user.email=study@example.invalid", "commit", "-qm", "Initial")

    def git(self, *arguments):
        return subprocess.check_output([program("git"), *arguments], cwd=self.source, text=True, shell=False).strip()  # nosemgrep  # nosec B603

    def test_committed_identity_allows_unrelated_untracked_notes(self):
        (self.source / "personal-notes").write_text("keep me\n")
        identity = study.source_identity(self.source)
        self.assertEqual(identity["commit"], self.git("rev-parse", "HEAD"))
        self.assertEqual(identity["tree"], self.git("rev-parse", "HEAD^{tree}"))
        self.assertEqual((self.source / "personal-notes").read_text(), "keep me\n")

    def test_uncommitted_or_untracked_build_inputs_are_rejected(self):
        (self.benchmark / "lsmctrainingstudy.cpp").write_text("changed sample\n")
        with self.assertRaisesRegex(ValueError, "committed"):
            study.source_identity(self.source)
        self.git("checkout", "--", ".")
        (self.benchmark / "new.cpp").write_text("untracked source\n")
        with self.assertRaisesRegex(ValueError, "untracked"):
            study.source_identity(self.source)

    def test_output_cannot_overlap_source_or_an_existing_directory(self):
        for path in (self.source / "study", self.source, self.source.parent):
            with self.subTest(path=path), self.assertRaises(ValueError):
                study.validate_destination(self.source, path)
        target = self.source.parent / "new-study"
        self.assertEqual(study.validate_destination(self.source, target), target)

    def test_dependency_must_be_clean_and_match_the_committed_pin(self):
        dependency = self.source.parent / "dependency"
        subprocess.run([program("git"), "clone", "-q", "--no-local", str(self.source), str(dependency)], check=True, shell=False)  # nosemgrep  # nosec B603
        pin = self.git("rev-parse", "HEAD")
        study.validate_dependency(dependency, pin)
        with self.assertRaisesRegex(ValueError, "pin"):
            study.validate_dependency(dependency, "0" * 40)
        (dependency / "dal-cpp/benchmarks/script_mc_perf/lsmctrainingstudy.cpp").write_text("dirty\n")
        with self.assertRaisesRegex(ValueError, "clean"):
            study.validate_dependency(dependency, pin)


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""Tests for the CI change classifier and workflow fast-path contract."""

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "classify_ci_changes.py"
SPEC = importlib.util.spec_from_file_location("classify_ci_changes", SCRIPT)
CLASSIFIER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CLASSIFIER)


class ClassifyCiChangesTest(unittest.TestCase):
    def test_markdown_and_document_assets_are_docs_only(self):
        self.assertTrue(
            CLASSIFIER.docs_only(
                (
                    "README.md",
                    "CHANGELOG.md",
                    "docs/introduction.md",
                    "docs/images/runtime.svg",
                    "examples/README.md",
                    "LICENSE",
                    "NOTICE",
                )
            )
        )

    def test_source_or_configuration_change_requires_full_ci(self):
        for paths in (
            ("docs/introduction.md", "dal-cpp/dal/math/matrix/matrix.cpp"),
            (".github/workflows/cmake-linux.yml",),
            ("CMakeLists.txt",),
            (".codex/agents/dal-tester.toml",),
        ):
            with self.subTest(paths=paths):
                self.assertFalse(CLASSIFIER.docs_only(paths))

    def test_empty_or_noncanonical_change_set_requires_full_ci(self):
        for paths in ((), ("../README.md",), ("docs/../CMakeLists.txt",)):
            with self.subTest(paths=paths):
                self.assertFalse(CLASSIFIER.docs_only(paths))

    @mock.patch.object(CLASSIFIER.subprocess, "run")
    def test_changed_paths_uses_nul_safe_fail_closed_git_diff(self, run):
        run.return_value = mock.Mock(stdout=b"src/lib.cpp\0docs/lib.md\0")

        self.assertEqual(
            CLASSIFIER.changed_paths("base", "head"),
            ("src/lib.cpp", "docs/lib.md"),
        )
        run.assert_called_once_with(
            (
                "git",
                "diff",
                "--no-renames",
                "--name-only",
                "-z",
                "base",
                "head",
                "--",
            ),
            cwd=CLASSIFIER.ROOT,
            check=True,
            capture_output=True,
        )

    @mock.patch.object(CLASSIFIER.subprocess, "run")
    def test_zero_push_base_requires_full_ci(self, run):
        self.assertEqual(CLASSIFIER.changed_paths("0" * 40, "head"), ())
        run.assert_not_called()

    @mock.patch.object(CLASSIFIER, "changed_paths")
    def test_main_appends_lowercase_github_output(self, changed_paths):
        changed_paths.return_value = ("docs/introduction.md",)
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "github-output"

            CLASSIFIER.main(
                (
                    "--base",
                    "base",
                    "--head",
                    "head",
                    "--github-output",
                    str(output),
                )
            )

            self.assertEqual(output.read_text(encoding="utf-8"), "docs_only=true\n")

    @mock.patch.object(CLASSIFIER, "changed_paths")
    def test_diff_failure_falls_back_to_full_ci(self, changed_paths):
        changed_paths.side_effect = CLASSIFIER.subprocess.CalledProcessError(
            128, ("git", "diff")
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "github-output"

            CLASSIFIER.main(
                (
                    "--base",
                    "missing-base",
                    "--head",
                    "head",
                    "--github-output",
                    str(output),
                )
            )

            self.assertEqual(output.read_text(encoding="utf-8"), "docs_only=false\n")


class CiWorkflowFastPathTest(unittest.TestCase):
    ROOT = Path(__file__).resolve().parents[3]

    @staticmethod
    def job(workflow, job_id):
        marker = f"  {job_id}:\n"
        start = workflow.index(marker) + len(marker)
        remainder = workflow[start:]
        next_job = remainder.find("\n  ")
        while next_job >= 0:
            candidate = remainder[next_job + 3 :].split("\n", 1)[0]
            if candidate.endswith(":") and " " not in candidate:
                return remainder[:next_job]
            next_job = remainder.find("\n  ", next_job + 3)
        return remainder

    def workflow(self, name):
        return (self.ROOT / ".github" / "workflows" / name).read_text(
            encoding="utf-8"
        )

    def test_workflows_classify_with_full_history(self):
        for name in ("cmake-linux.yml", "cmake-windows.yml"):
            with self.subTest(name=name):
                workflow = self.workflow(name)
                self.assertIn(
                    "on:\n  pull_request:\n  push:\n    branches:\n      - master\n",
                    workflow,
                )
                changes = self.job(workflow, "changes")
                self.assertIn("name: Classify changes", changes)
                self.assertIn("fetch-depth: 0", changes)
                self.assertIn("classify_ci_changes.py", changes)
                self.assertIn(
                    "docs_only: ${{ steps.classify.outputs.docs_only }}", changes
                )

    def test_release_workflow_excludes_component_documentation(self):
        workflow = self.workflow("dal-python-release.yml")
        positive_paths = ("dal-python/**", "dal-cpp/**", "dal-public/**")
        for positive in positive_paths:
            with self.subTest(positive=positive):
                self.assertIn(f"      - {positive}\n", workflow)
                positive_offset = workflow.index(f"      - {positive}\n")
                for suffix in ("md", "mdx", "rst"):
                    negative = f"      - '!{positive}/*.{suffix}'\n"
                    self.assertIn(negative, workflow)
                    self.assertGreater(workflow.index(negative), positive_offset)

    def test_linux_fast_path_preserves_docs_and_stable_gate(self):
        workflow = self.workflow("cmake-linux.yml")
        heavy_jobs = (
            "define-matrix",
            "build",
            "codipack-thread-isolation",
            "build-extended",
            "warning-clean",
            "sanitizers",
            "benchmark",
        )
        for job_id in heavy_jobs:
            with self.subTest(job_id=job_id):
                job = self.job(workflow, job_id)
                self.assertIn("changes", job)
                self.assertIn("needs.changes.outputs.docs_only != 'true'", job)

        documentation = self.job(workflow, "documentation")
        self.assertIn("needs: changes", documentation)
        self.assertNotIn("needs.changes.outputs.docs_only", documentation)

        gate = self.job(workflow, "linux-gate")
        self.assertIn("name: Linux CI gate", gate)
        self.assertIn("if: always()", gate)
        self.assertIn('test "$CHANGES_RESULT" = success', gate)
        self.assertIn('test "$DOCS_RESULT" = success', gate)
        self.assertIn('if [ "$DOCS_ONLY" = true ]; then', gate)
        for job_id in heavy_jobs:
            with self.subTest(gate_dependency=job_id):
                self.assertIn(f"needs.{job_id}.result", gate)

    def test_windows_fast_path_preserves_stable_gate(self):
        workflow = self.workflow("cmake-windows.yml")
        heavy_jobs = ("build", "build-script", "benchmark")
        for job_id in heavy_jobs:
            with self.subTest(job_id=job_id):
                job = self.job(workflow, job_id)
                self.assertIn("needs: changes", job)
                self.assertIn("needs.changes.outputs.docs_only != 'true'", job)

        gate = self.job(workflow, "windows-gate")
        self.assertIn("name: Windows CI gate", gate)
        self.assertIn("if: always()", gate)
        self.assertIn('test "$CHANGES_RESULT" = success', gate)
        self.assertIn('if [ "$DOCS_ONLY" = true ]; then', gate)
        for job_id in heavy_jobs:
            with self.subTest(gate_dependency=job_id):
                self.assertIn(f"needs.{job_id}.result", gate)

    def test_benchmark_jobs_persist_environment_and_results(self):
        for workflow_name in ("cmake-linux.yml", "cmake-windows.yml"):
            with self.subTest(workflow=workflow_name):
                benchmark = self.job(self.workflow(workflow_name), "benchmark")
                self.assertIn("write_benchmark_metadata.py", benchmark)
                self.assertIn("--head-sha", benchmark)
                self.assertIn("--base-sha", benchmark)
                self.assertIn("--aad-backend aadet", benchmark)
                self.assertIn("--threads 4", benchmark)
                self.assertIn("actions/upload-artifact@", benchmark)
                self.assertIn("if: always()", benchmark)
                self.assertIn("path: benchmark-results", benchmark)
                self.assertIn("retention-days: 30", benchmark)

        linux_benchmark = self.job(self.workflow("cmake-linux.yml"), "benchmark")
        self.assertIn("/usr/bin/time --verbose", linux_benchmark)
        self.assertIn('resource_file="benchmark-results/${bench}.resources.txt"', linux_benchmark)

    def test_linux_benchmark_pairs_pull_requests_and_master_pushes(self):
        benchmark = self.job(self.workflow("cmake-linux.yml"), "benchmark")

        self.assertIn(
            "ref: ${{ github.event_name == 'pull_request' && github.event.pull_request.base.sha || github.event.before }}",
            benchmark,
        )
        self.assertNotIn("if: github.event_name == 'pull_request'", benchmark)


if __name__ == "__main__":
    unittest.main()

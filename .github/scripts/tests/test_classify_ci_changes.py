"""Tests for the CI change classifier and workflow fast-path contract."""

import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
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

            self.assertEqual(
                output.read_text(encoding="utf-8"),
                "docs_only=true\n",
            )

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

            self.assertEqual(
                output.read_text(encoding="utf-8"),
                "docs_only=false\n",
            )


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

    def test_benchmarks_are_not_part_of_pull_request_or_push_ci(self):
        for name, gate_id in (
            ("cmake-linux.yml", "linux-gate"),
            ("cmake-windows.yml", "windows-gate"),
        ):
            with self.subTest(name=name):
                workflow = self.workflow(name)
                self.assertNotIn("  benchmark:\n", workflow)
                self.assertNotIn("benchmark_needed", workflow)
                self.assertNotIn("needs.benchmark", self.job(workflow, gate_id))

    def test_scheduled_benchmarks_keep_both_platforms_and_artifacts(self):
        workflow = self.workflow("benchmarks.yml")
        self.assertIn("cron: '0 6,18 * * *'", workflow)
        self.assertIn("timezone: Asia/Shanghai", workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertNotIn("pull_request:", workflow)
        self.assertNotIn("  push:\n", workflow)

        linux = self.job(workflow, "linux")
        windows = self.job(workflow, "windows")
        self.assertIn("check_python_benchmark_regressions.py", linux)
        self.assertIn("check_benchmark_regressions.py", linux)
        self.assertIn("run_comparisons.py", linux)
        self.assertIn("ctest -N -L benchmark", windows)
        self.assertIn("fetch-depth: 2", windows)
        self.assertIn("actions/upload-artifact@", linux)
        self.assertIn("actions/upload-artifact@", windows)
        self.assertNotIn("  report:\n", workflow)
        self.assertNotIn("BENCHMARK_SMTP", workflow)

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

    def test_release_workflow_runs_only_for_release_tags(self):
        workflow = self.workflow("dal-python-release.yml")
        self.assertIn("on:\n  push:\n    tags:\n      - dal-python-v*\n", workflow)
        self.assertNotIn("  pull_request:", workflow)
        self.assertNotIn("  workflow_dispatch:", workflow)

    def test_pull_requests_build_and_test_python_wheels_outside_release(self):
        workflow = self.workflow("dal-python-ci.yml")
        self.assertIn("on:\n  pull_request:\n    paths:\n", workflow)
        for component in ("dal-python", "dal-cpp", "dal-public"):
            self.assertIn(f"      - {component}/**\n", workflow)
            for suffix in ("md", "mdx", "rst"):
                self.assertIn(f"      - '!{component}/**/*.{suffix}'\n", workflow)
        self.assertIn("      - .github/workflows/dal-python-ci.yml\n", workflow)
        self.assertNotIn("gh-action-pypi-publish", workflow)
        wheels = self.job(workflow, "wheels")
        self.assertIn("CIBW_BUILD: cp39-* cp314-*", wheels)
        for platform in ("linux-x86_64", "windows-amd64", "macos-x86_64", "macos-arm64"):
            self.assertIn(f"platform: {platform}", wheels)
        self.assertIn("pypa/cibuildwheel@", wheels)
        self.assertIn("smoke_installed_wheel.py", wheels)
        verify = self.job(workflow, "verify")
        self.assertIn("needs: wheels", verify)
        self.assertIn("--expected-python cp39,cp314", verify)
        self.assertIn("verify_release.py", verify)

    def test_linux_fast_path_preserves_docs_and_stable_gate(self):
        workflow = self.workflow("cmake-linux.yml")
        heavy_jobs = (
            "define-matrix",
            "build",
            "codipack-thread-isolation",
            "build-extended",
            "warning-clean",
            "sanitizers",
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
        heavy_jobs = ("build", "build-script")
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
        workflow = self.workflow("benchmarks.yml")
        for job_id in ("linux", "windows"):
            with self.subTest(job_id=job_id):
                benchmark = self.job(workflow, job_id)
                self.assertIn("write_benchmark_metadata.py", benchmark)
                self.assertIn("--head-sha", benchmark)
                self.assertIn("--base-sha", benchmark)
                self.assertIn("--aad-backend aadet", benchmark)
                self.assertIn("--threads 4", benchmark)
                self.assertIn("actions/upload-artifact@", benchmark)
                self.assertIn("if: always()", benchmark)
                self.assertIn("path: benchmark-results", benchmark)
                self.assertIn("retention-days: 30", benchmark)

        linux_benchmark = self.job(workflow, "linux")
        self.assertIn("/usr/bin/time --verbose", linux_benchmark)
        self.assertIn('resource_file="benchmark-results/${bench}.resources.txt"', linux_benchmark)

    def run_gate(self, platform, results, docs_only=False):
        gate = self.job(self.workflow(f"cmake-{platform}.yml"), f"{platform}-gate")
        bindings = re.findall(r"^      (\w+): \$\{\{ needs\.([\w-]+)\.result \}\}$", gate, re.MULTILINE)
        environment = {
            "PATH": os.environ.get("PATH", ""),
            "DOCS_ONLY": str(docs_only).lower(),
        }
        environment.update({name: results.get(job, "") for name, job in bindings})
        script = textwrap.dedent(gate.split("        run: |\n", 1)[1])
        return subprocess.run(
            ["bash", "--noprofile", "--norc", "-e", "-o", "pipefail", "-c", script],
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        ).returncode

    @unittest.skipIf(sys.platform == "win32" or not shutil.which("bash"), "CI gates execute on Linux with bash")
    def test_gate_shell_requires_success_for_every_required_job(self):
        required_jobs = {
            "linux": ("changes", "documentation", "define-matrix", "build",
                      "codipack-thread-isolation", "build-extended", "warning-clean", "sanitizers"),
            "windows": ("changes", "build", "build-script"),
        }
        for platform, jobs in required_jobs.items():
            success = dict.fromkeys(jobs, "success")
            with self.subTest(platform=platform, result="success"):
                self.assertEqual(self.run_gate(platform, success), 0)
            for job in jobs:
                for result in ("failure", "cancelled", "skipped", "running", ""):
                    with self.subTest(platform=platform, job=job, result=result):
                        self.assertNotEqual(self.run_gate(platform, {**success, job: result}), 0)

    @unittest.skipIf(sys.platform == "win32" or not shutil.which("bash"), "CI gates execute on Linux with bash")
    def test_gate_shell_docs_only_accepts_skipped_builds_and_checks_documentation(self):
        for platform in ("linux", "windows"):
            results = dict.fromkeys(("define-matrix", "build", "codipack-thread-isolation",
                                     "build-extended", "warning-clean", "sanitizers",
                                     "build-script"), "skipped")
            required = ("changes", "documentation") if platform == "linux" else ("changes",)
            results.update(dict.fromkeys(required, "success"))
            with self.subTest(platform=platform):
                self.assertEqual(self.run_gate(platform, results, docs_only=True), 0)
                self.assertNotEqual(self.run_gate(platform, results), 0)
            for job in required:
                for result in ("failure", "cancelled", "skipped", "running", ""):
                    with self.subTest(platform=platform, job=job, result=result):
                        self.assertNotEqual(self.run_gate(platform, {**results, job: result}, docs_only=True), 0)

    def test_scheduled_linux_benchmark_compares_with_previous_master_commit(self):
        benchmark = self.job(self.workflow("benchmarks.yml"), "linux")

        self.assertIn('git rev-parse HEAD^1', benchmark)
        self.assertIn("ref: ${{ env.BASE_SHA }}", benchmark)


if __name__ == "__main__":
    unittest.main()

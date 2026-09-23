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

    def test_example_source_and_documentation_changes_skip_benchmarks(self):
        for paths in (
            ("dal-cpp/examples/american_put_mc/american_put_mc.cpp",),
            ("dal-cpp/examples/uoc/uoc.cpp", "docs/methodology/script_engine.md"),
            ("README.md",),
        ):
            with self.subTest(paths=paths):
                self.assertFalse(CLASSIFIER.benchmark_needed(paths))

    def test_core_code_configuration_and_unknown_paths_need_benchmarks(self):
        for paths in (
            (),
            ("dal-cpp/dal/script/simulation.cpp",),
            ("dal-cpp/examples/uoc/uoc.cpp", "dal-cpp/dal/script/simulation.cpp"),
            ("dal-cpp/examples/CMakeLists.txt",),
            (".github/workflows/cmake-linux.yml",),
            ("../dal-cpp/examples/uoc/uoc.cpp",),
            ("dal-cpp/examples/../dal/script/simulation.cpp",),
        ):
            with self.subTest(paths=paths):
                self.assertTrue(CLASSIFIER.benchmark_needed(paths))

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
                "docs_only=true\nbenchmark_needed=false\n",
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
                "docs_only=false\nbenchmark_needed=true\n",
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
                self.assertIn(
                    "benchmark_needed: ${{ steps.classify.outputs.benchmark_needed }}",
                    changes,
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

    def test_performance_reports_block_required_gates(self):
        for workflow_name, gate_id in (
            ("cmake-linux.yml", "linux-gate"),
            ("cmake-windows.yml", "windows-gate"),
        ):
            with self.subTest(workflow=workflow_name):
                gate = self.job(self.workflow(workflow_name), gate_id)
                self.assertIn("- benchmark", gate)
                self.assertIn("needs.benchmark.result", gate)
                self.assertIn("BENCHMARK_RESULT", gate)
                self.assertIn("BENCHMARK_NEEDED", gate)
                benchmark = self.job(self.workflow(workflow_name), "benchmark")
                self.assertIn("needs.changes.outputs.benchmark_needed == 'true'", benchmark)

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

    def run_gate(self, platform, results, docs_only=False, benchmark_needed=True):
        gate = self.job(self.workflow(f"cmake-{platform}.yml"), f"{platform}-gate")
        bindings = re.findall(r"^      (\w+): \$\{\{ needs\.([\w-]+)\.result \}\}$", gate, re.MULTILINE)
        environment = {
            "PATH": os.environ.get("PATH", ""),
            "DOCS_ONLY": str(docs_only).lower(),
            "BENCHMARK_NEEDED": str(benchmark_needed).lower(),
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
                      "codipack-thread-isolation", "build-extended", "warning-clean", "sanitizers",
                      "benchmark"),
            "windows": ("changes", "build", "build-script", "benchmark"),
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
                                     "build-script", "benchmark"), "skipped")
            required = ("changes", "documentation") if platform == "linux" else ("changes",)
            results.update(dict.fromkeys(required, "success"))
            with self.subTest(platform=platform):
                self.assertEqual(self.run_gate(platform, results, docs_only=True), 0)
                self.assertNotEqual(self.run_gate(platform, results), 0)
            for job in required:
                for result in ("failure", "cancelled", "skipped", "running", ""):
                    with self.subTest(platform=platform, job=job, result=result):
                        self.assertNotEqual(self.run_gate(platform, {**results, job: result}, docs_only=True), 0)

    @unittest.skipIf(sys.platform == "win32" or not shutil.which("bash"), "CI gates execute on Linux with bash")
    def test_gate_shell_example_only_requires_skipped_benchmark_and_other_checks(self):
        required_jobs = {
            "linux": ("changes", "documentation", "define-matrix", "build",
                      "codipack-thread-isolation", "build-extended", "warning-clean", "sanitizers"),
            "windows": ("changes", "build", "build-script"),
        }
        for platform, jobs in required_jobs.items():
            results = {**dict.fromkeys(jobs, "success"), "benchmark": "skipped"}
            with self.subTest(platform=platform, result="success"):
                self.assertEqual(self.run_gate(platform, results, benchmark_needed=False), 0)
                self.assertNotEqual(self.run_gate(platform, results, benchmark_needed=""), 0)
            for result in ("success", "failure", "cancelled", "running", ""):
                with self.subTest(platform=platform, benchmark=result):
                    self.assertNotEqual(
                        self.run_gate(platform, {**results, "benchmark": result}, benchmark_needed=False),
                        0,
                    )
            for job in jobs:
                with self.subTest(platform=platform, required_job=job):
                    self.assertNotEqual(
                        self.run_gate(platform, {**results, job: "failure"}, benchmark_needed=False),
                        0,
                    )

    def test_linux_benchmark_pairs_pull_requests_and_master_pushes(self):
        benchmark = self.job(self.workflow("cmake-linux.yml"), "benchmark")

        self.assertIn("PR_BASE_SHA: ${{ github.event.pull_request.base.sha }}", benchmark)
        self.assertIn("EVENT_BEFORE: ${{ github.event.before }}", benchmark)
        self.assertIn('base_sha="${PR_BASE_SHA:-$EVENT_BEFORE}"', benchmark)
        self.assertIn('[[ "$base_sha" =~ ^0+$ ]]', benchmark)
        self.assertIn("ref: ${{ env.BASE_SHA }}", benchmark)
        self.assertNotIn("if: github.event_name == 'pull_request'", benchmark)


if __name__ == "__main__":
    unittest.main()

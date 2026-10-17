"""Exercise scheduled benchmark commands through a real CTest inventory."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest


SCRIPTS = Path(__file__).resolve().parents[1]
RUNNER = SCRIPTS / "run_benchmarks.py"
WORKFLOW = SCRIPTS.parent / "workflows/benchmarks.yml"
PROFILE_ARGS = ["--compiled-soa", "weekly", "32", "65", "1", "steady"]


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest"), "requires CMake and CTest")
class RunBenchmarksTest(unittest.TestCase):
    def configure(self, root, build, failure=False, empty=False):
        source = root / "fixture source"
        source.mkdir()
        working = root / "case working directory"
        working.mkdir()
        worker = source / "benchmark worker.py"
        worker.write_text(
            "import json, os, sys\n"
            "print(json.dumps({'args': sys.argv[1:], 'cwd': os.getcwd(), "
            "'threads': os.getenv('DAL_NUM_THREADS')}), flush=True)\n"
            "print('Fixture case     1.0 us     1.0 us     1.0 us     1')\n"
            "print('fixture stderr', file=sys.stderr)\n"
            "sys.exit(23 if '--fail' in sys.argv else 0)\n",
            encoding="utf-8",
        )
        command = f'"{Path(sys.executable).as_posix()}" "{worker.as_posix()}"'
        cases = ""
        if not empty:
            cases = f"""
add_test(NAME ordinary_perf COMMAND {command} {"--fail" if failure else ""})
add_test(NAME script_mc_compiled_soa_profile COMMAND {command} {" ".join(PROFILE_ARGS)})
set_tests_properties(ordinary_perf script_mc_compiled_soa_profile PROPERTIES
    LABELS "benchmark" WORKING_DIRECTORY "{working.as_posix()}")
"""
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(BenchmarkRunnerFixture LANGUAGES NONE)\n"
            "enable_testing()\n"
            f"add_test(NAME excluded COMMAND {command} --fail)\n" + cases,
            encoding="utf-8",
        )
        subprocess.run(
            ["cmake", "-S", str(source), "-B", str(build)],
            check=True, capture_output=True, text=True,
        )
        return working

    def run_cli(self, root, build, *extra):
        # Launch this repository's runner with fixture paths as separate arguments.
        return subprocess.run(  # nosemgrep
            [sys.executable, str(RUNNER), "--build-root", str(build),
             "--results-dir", str(root / "results"), "--configuration", "fixture", *extra],
            cwd=root, env={**os.environ, "DAL_NUM_THREADS": "4"},
            capture_output=True, text=True, check=False, shell=False,
        )

    def assert_profile(self, results, working):
        output = (results / "script_mc_compiled_soa_profile.txt").read_text()
        record = json.loads(output.splitlines()[0])
        self.assertEqual(record["args"], PROFILE_ARGS)
        self.assertEqual(Path(record["cwd"]), working)
        self.assertEqual(record["threads"], "4")
        self.assertIn("fixture stderr", output)
        self.assertEqual((results / "script_mc_compiled_soa_profile.status").read_text(), "0\n")
        self.assertFalse((results / "excluded.status").exists())

    @unittest.skipUnless(shutil.which("bash") and sys.platform == "linux", "requires Linux bash and GNU time")
    def test_scheduled_steps_execute_registered_command_and_arguments(self):
        workflow = WORKFLOW.read_text()
        for step_name, build_path in (
            ("Run head benchmarks", "build/benchmark-head"),
            ("Run benchmarks", "build/Release-windows"),
        ):
            with self.subTest(step=step_name), tempfile.TemporaryDirectory(prefix="dal benchmark runner ") as directory:
                root = Path(directory)
                shutil.copytree(SCRIPTS, root / ".github/scripts", ignore=shutil.ignore_patterns("__pycache__"))
                working = self.configure(root, root / build_path)
                step = workflow.split(f"      - name: {step_name}\n", 1)[1].split("      - name:", 1)[0]
                script = textwrap.dedent(step.split("        run: |\n", 1)[1])
                summary = root / "step-summary.md"
                completed = subprocess.run(
                    ["bash", "-e", "-o", "pipefail", "-c", script], cwd=root,
                    env={**os.environ, "GITHUB_STEP_SUMMARY": str(summary), "DAL_NUM_THREADS": "4"},
                    capture_output=True, text=True, check=False,
                )
                self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
                results = root / "benchmark-results"
                self.assert_profile(results, working)
                self.assertIn("| script_mc_compiled_soa_profile | Fixture case |", summary.read_text())
                if step_name == "Run head benchmarks":
                    self.assertIn("Maximum resident set size", (results / "script_mc_compiled_soa_profile.resources.txt").read_text())

    def test_failure_retains_output_and_status_and_runs_remaining_cases(self):
        with tempfile.TemporaryDirectory(prefix="dal benchmark failure ") as directory:
            root = Path(directory)
            build = root / "build"
            working = self.configure(root, build, failure=True)
            completed = self.run_cli(root, build)
            self.assertEqual(completed.returncode, 1, completed.stdout + completed.stderr)
            results = root / "results"
            self.assertEqual((results / "ordinary_perf.status").read_text(), "23\n")
            self.assertIn("fixture stderr", (results / "ordinary_perf.txt").read_text())
            self.assert_profile(results, working)
            self.assertIn("`ordinary_perf` (exit code 23)", (results / "summary.md").read_text())

    def test_empty_inventory_fails_without_a_success_summary(self):
        with tempfile.TemporaryDirectory(prefix="dal benchmark empty ") as directory:
            root = Path(directory)
            build = root / "build"
            self.configure(root, build, empty=True)
            completed = self.run_cli(root, build)
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("no benchmark tests", completed.stderr)
            self.assertFalse((root / "results/summary.md").exists())

    def test_discovery_failure_is_not_reported_as_a_pass(self):
        with tempfile.TemporaryDirectory(prefix="dal benchmark discovery ") as directory:
            root = Path(directory)
            completed = self.run_cli(root, root / "missing build")
            self.assertNotEqual(completed.returncode, 0)
            self.assertFalse((root / "results/summary.md").exists())


if __name__ == "__main__":
    unittest.main()

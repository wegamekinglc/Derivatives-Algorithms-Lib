"""Contracts for the manual, fixed-comparison DAL-469 diagnostic job."""

import importlib.util
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import tempfile
import textwrap
from types import SimpleNamespace
import unittest
from unittest import mock


SCRIPTS = Path(__file__).resolve().parents[1]
WORKFLOW = SCRIPTS.parent / "workflows/benchmarks.yml"
sys.path.insert(0, str(SCRIPTS))


def load_diagnostic():
    spec = importlib.util.spec_from_file_location("diagnose_dal469", SCRIPTS / "diagnose_dal469.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class DiagnosticWorkflowTest(unittest.TestCase):
    def test_diagnostic_mode_is_manual_and_isolated_from_scheduled_jobs(self):
        workflow = WORKFLOW.read_text()
        dispatch = workflow.split("  workflow_dispatch:", 1)[1].split("\npermissions:", 1)[0]
        self.assertIn("      mode:", dispatch)
        self.assertIn("default: scheduled", dispatch)
        self.assertIn("          - dal-469", dispatch)
        jobs = dict(re.findall(r"^  (\w+):\n(.*?)(?=^  \w+:\n|\Z)",
                               workflow.split("\njobs:\n", 1)[1], re.M | re.S))
        self.assertIn("diagnostic", jobs)
        self.assertIn("github.event_name == 'workflow_dispatch' && inputs.mode == 'dal-469'",
                      jobs["diagnostic"])
        self.assertIn("runs-on: ubuntu-24.04", jobs["diagnostic"])
        for name in ("linux", "windows"):
            self.assertIn("github.event_name != 'workflow_dispatch' || inputs.mode == 'scheduled'", jobs[name])

    def test_task_ref_controller_fixed_sources_and_always_upload_are_retained(self):
        job = WORKFLOW.read_text().split("\n  diagnostic:\n", 1)[1]
        self.assertIn("ref: 72b3d15e04a9a42859b3f3b264b0f4ea635ddb20", job)
        self.assertIn("ref: d7eaefb7808ba32aecc870b7babc7cc9e7fc650a", job)
        self.assertNotIn("ref: master", job)
        self.assertIn("--python 3.13.17", job)
        self.assertIn("cmake==3.31.6", job)
        upload = job.split("- name: Upload diagnostic evidence", 1)[1]
        self.assertIn("if: always()", upload)
        self.assertIn("path: diagnostic-results", upload)
        self.assertIn("if-no-files-found: error", upload)
        self.assertNotIn("continue-on-error", job)
        self.assertNotRegex(job, r"run:.*\$\{\{\s*inputs\.")

    @unittest.skipUnless(sys.platform == "linux", "requires Linux bash")
    def test_build_step_uses_only_selected_targets_with_literal_paths(self):
        workflow = WORKFLOW.read_text().split("- name: Build only diagnostic targets", 1)[1]
        script = textwrap.dedent(workflow.split("        run: |\n", 1)[1].split("      - name:", 1)[0])
        for mode in ("original", "rebuild"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="dal paths ; literal ") as directory:
                root = Path(directory)
                (root / "diagnostic-results").mkdir()
                command = root / "cmake"
                command.write_text(f"#!{sys.executable}\nimport json,os,sys\n"
                                   "with open(os.environ['TRACE'], 'a') as f: f.write(json.dumps(sys.argv[1:])+'\\n')\n")
                command.chmod(0o755)
                result = subprocess.run(["bash", "-e", "-o", "pipefail", "-c", script], cwd=root,
                                        env={**os.environ, "PATH": f"{root}:{os.environ['PATH']}",
                                             "TRACE": str(root / "trace"), "DAL469_PYTHON_MODE": mode,
                                             "CMAKE_BUILD_PARALLEL_LEVEL": "2"}, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                calls = [json.loads(line) for line in (root / "trace").read_text().splitlines()]
                self.assertEqual(len(calls), 4)
                for args in (calls[1], calls[3]):
                    targets = args[args.index("--target") + 1:args.index("--parallel")]
                    self.assertEqual(targets, ["tape_perf", "_dal"] if mode == "rebuild" else ["tape_perf"])
                self.assertEqual(calls[0][calls[0].index("-S") + 1], str(root / "diagnostic-workspace"))
                executable = next(arg.split("=", 1)[1] for arg in calls[0] if arg.startswith("-DPython3_EXECUTABLE="))
                self.assertEqual(Path(executable).resolve(), Path(sys.executable).resolve())


class DiagnosticCommandTest(unittest.TestCase):
    def test_nonmanual_or_unknown_modes_fail_before_running_commands(self):
        diagnostic = load_diagnostic()
        for event, mode in (("schedule", "dal-469"), ("workflow_dispatch", "scheduled"),
                            ("workflow_dispatch", "dal-469; touch injected")):
            with self.subTest(event=event, mode=mode), mock.patch.dict(
                os.environ, {"GITHUB_EVENT_NAME": event, "DAL469_MODE": mode}
            ), mock.patch.object(diagnostic, "run_command") as command:
                with self.assertRaisesRegex(ValueError, "manual dal-469"):
                    diagnostic.main(["prepare"])
                command.assert_not_called()

    def test_command_arguments_are_literal_and_status_and_output_are_retained(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory(prefix="dal diagnostic ; ") as directory:
            root = Path(directory)
            argument = "$(touch injected); literal argument"
            code = "import json,sys; print(json.dumps(sys.argv[1:])); sys.exit(23)"
            status = diagnostic.run_command([sys.executable, "-c", code, argument], root / "command.log")
            self.assertEqual(status, 23)
            self.assertEqual(json.loads((root / "command.log").read_text()), [argument])
            record = json.loads((root / "command.command.json").read_text())
            self.assertEqual(record["returncode"], 23)
            self.assertEqual(record["argv"][-1], argument)
            self.assertFalse((root / "injected").exists())

    def test_archive_rejects_traversal_and_links_before_extracting_any_file(self):
        diagnostic = load_diagnostic()
        for name, kind in (("../escaped", tarfile.REGTYPE), ("dal/link", tarfile.SYMTYPE), (".", tarfile.REGTYPE)):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                archive = root / "package.tar.gz"
                with tarfile.open(archive, "w:gz") as stream:
                    member = tarfile.TarInfo(name)
                    member.type = kind
                    member.linkname = "../../escaped" if kind == tarfile.SYMTYPE else ""
                    member.size = 1 if kind == tarfile.REGTYPE else 0
                    stream.addfile(member, io.BytesIO(b"x"))
                with self.assertRaisesRegex(ValueError, "unsafe archive"):
                    diagnostic.unpack_package(archive, root / "package")
                self.assertFalse((root / "escaped").exists())

    def test_missing_isa_selects_paired_rebuild_without_executing_original_modules(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(diagnostic, "run_command") as run:
            root = Path(directory)
            result = diagnostic.choose_packages(root, root / "evidence", set(), (3, 13, 17))
            self.assertEqual(result["python_mode"], "rebuild")
            self.assertIn("ISA", result["reason"])
            self.assertEqual(set(result["packages"]), {"base", "head"})
            run.assert_not_called()

    def test_sampling_failure_is_retained_while_the_other_target_still_runs(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            with mock.patch.object(diagnostic, "run_tape", return_value=23) as tape, \
                    mock.patch.object(diagnostic, "run_prefix", return_value=0) as prefix:
                status = diagnostic.measure_targets(output, {}, {})
            self.assertEqual(status, 23)
            tape.assert_called_once()
            prefix.assert_called_once()
            result = json.loads((output / "measurement-status.json").read_text())
            self.assertEqual(result["tape"], 23)
            self.assertEqual(result["python-prefix"], 0)
            self.assertEqual(result["original_ci_exit_status"], 1)

    def test_timeout_retains_output_and_failure_status(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "timeout.log"
            status = diagnostic.run_command([sys.executable, "-c", "import time; time.sleep(5)"], log, timeout=0.01)
            self.assertEqual(status, 124)
            self.assertIn("TimeoutExpired", log.read_text())
            self.assertEqual(json.loads(log.with_suffix(".command.json").read_text())["returncode"], 124)

    def test_profiles_do_not_replace_failed_measurement_status(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            diagnostic.write_json(root / "selection.json", {"packages": {"base": "base", "head": "head"}})
            diagnostic.write_json(root / "measurement-status.json", {"tape": 23, "python-prefix": 0})
            with mock.patch.object(diagnostic, "run_command", return_value=7), \
                    mock.patch.object(diagnostic.shutil, "which", return_value=None):
                self.assertEqual(diagnostic.profile(root, root), 0)
            self.assertEqual(json.loads((root / "measurement-status.json").read_text())["tape"], 23)
            self.assertTrue((root / "profiles/status.json").exists())

    def test_original_packages_require_hashes_and_handle_isa_failure_as_a_pair(self):
        diagnostic = load_diagnostic()
        for statuses in ((0, 0), (-4, 0), (0, 78), (1,)):
            with self.subTest(statuses=statuses), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                original = root / "dal469-original/python-reproduction"
                original.mkdir(parents=True)
                for side in ("base", "head"):
                    with tarfile.open(original / f"{side}-package.tar.gz", "w:gz") as archive:
                        member = tarfile.TarInfo("dal/_dal.cpython-313-x86_64-linux-gnu.so")
                        member.size = 1
                        archive.addfile(member, io.BytesIO(b"x"))
                hashes = {side: diagnostic.hashlib.sha256(b"x").hexdigest() for side in ("base", "head")}
                with mock.patch.dict(os.environ, {"RUNNER_TEMP": str(root)}), \
                        mock.patch.object(diagnostic, "ORIGINAL_HASHES", hashes), \
                        mock.patch.object(diagnostic, "run_command", side_effect=statuses):
                    if statuses == (1,):
                        with self.assertRaisesRegex(ValueError, "correctness/probe failed"):
                            diagnostic.choose_packages(root, root / "evidence", diagnostic.REQUIRED_ISA, (3, 13, 17))
                    else:
                        result = diagnostic.choose_packages(root, root / "evidence", diagnostic.REQUIRED_ISA, (3, 13, 17))
                        self.assertEqual(result["python_mode"], "original" if statuses == (0, 0) else "rebuild")
                        self.assertEqual(set(result["packages"]), {"base", "head"})

    def test_both_original_hashes_are_verified_before_either_binary_is_executed(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "dal469-original/python-reproduction"
            original.mkdir(parents=True)
            for side in ("base", "head"):
                with tarfile.open(original / f"{side}-package.tar.gz", "w:gz") as archive:
                    member = tarfile.TarInfo("dal/_dal.cpython-313-x86_64-linux-gnu.so")
                    member.size = 1
                    archive.addfile(member, io.BytesIO(b"x"))
            hashes = {"base": diagnostic.hashlib.sha256(b"x").hexdigest(), "head": "unexpected"}
            with mock.patch.dict(os.environ, {"RUNNER_TEMP": str(root)}), \
                    mock.patch.object(diagnostic, "ORIGINAL_HASHES", hashes), \
                    mock.patch.object(diagnostic, "run_command") as run:
                with self.assertRaisesRegex(ValueError, "head module hash mismatch"):
                    diagnostic.choose_packages(root, root / "evidence", diagnostic.REQUIRED_ISA, (3, 13, 17))
                run.assert_not_called()


class PrefixDiagnosticTest(unittest.TestCase):
    def test_malformed_worker_report_is_retained_as_failed_diagnostic(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "results"
            def malformed(command, log, **kwargs):
                log.parent.mkdir(parents=True)
                (log.parent / "results.json").write_text("[]")
                return 0
            with mock.patch.object(diagnostic, "run_command", side_effect=malformed):
                self.assertEqual(diagnostic.run_prefix({"head": {"source": Path(directory)}},
                                                      {"base": Path(directory)}, output), 1)
            result = json.loads((output / "results.json").read_text())
            self.assertEqual(result["status"], "failed")
            self.assertIn("TypeError", result["failures"][0])

    def test_prefix_order_and_inventory_are_fixed(self):
        import dal469_prefix_worker as worker
        cases = [SimpleNamespace(name=name) for name in worker.PREFIX]
        self.assertEqual(worker.select_prefix(cases + [SimpleNamespace(name="later")]), cases)
        for invalid in (cases[:-1], list(reversed(cases))):
            with self.assertRaisesRegex(ValueError, "prefix changed"):
                worker.select_prefix(invalid)

    def test_real_fixture_processes_preserve_sampling_and_existing_failure_verdict(self):
        import dal469_prefix_worker as worker
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory(prefix="dal prefix ; ") as directory:
            root = Path(directory)
            packages = {side: root / side for side in ("base", "head")}
            for package in packages.values():
                (package / "dal").mkdir(parents=True)
                (package / "dal/_dal.cpython-313-x86_64-linux-gnu.so").write_bytes(b"fixture")
            fixture = root / "worker.py"
            fixture.write_text("import hashlib,json,os,sys\nfrom pathlib import Path\n"
                               "package=Path(sys.argv[1]); output=Path(sys.argv[2]); output.mkdir(parents=True,exist_ok=True)\n"
                               "with open(package.parent/'trace','a') as f: f.write(package.name+'\\n')\n"
                               "value=105 if package.name=='head' else 100\n"
                               "report={'schema':'dal.python-benchmarks/1','profile':'full','status':'passed','samples':1,'warmups':2,"
                               "'environment':{'suite_sha256':{'cases.py':'suite'},'native_sha256':hashlib.sha256(b'fixture').hexdigest(),"
                               "'python':sys.version,'thread_environment':{'DAL_NUM_THREADS':os.environ['DAL_NUM_THREADS']}},'results':[]}\n"
                               f"for name in {worker.PREFIX!r}:\n"
                               " report['results'].append({'name':name,'cpp_target':'script_mc_perf','workload':{'profile':'full'},"
                               "'status':'passed','samples_ns':[value],'min_ns':value})\n"
                               "(output/'results.json').write_text(json.dumps(report))\n")
            def command(suite, package, destination):
                return [sys.executable, fixture, package, destination]
            with mock.patch.object(diagnostic, "prefix_command", side_effect=command):
                self.assertEqual(diagnostic.run_prefix({"head": {"source": root}}, packages, root / "results"), 1)
            order = (root / "trace").read_text().splitlines()
            self.assertEqual(order, [side for pair in range(20) for side in
                                     (("base", "head") if pair % 2 == 0 else ("head", "base"))])
            result = json.loads((root / "results/results.json").read_text())
            self.assertEqual(len(result["failures"]), 8)
            self.assertEqual(result["comparisons"][0]["round_paired_exceedances"], [10, 10])
            self.assertEqual(len(list((root / "results/raw").glob("*/results.json"))), 40)
            self.assertNotIn("Every current case is compared", (root / "results/summary.md").read_text())

    @unittest.skipUnless(sys.platform == "linux", "requires executable fixture scripts")
    def test_measurement_stage_retains_gate_failures_and_binary_evidence(self):
        diagnostic = load_diagnostic()
        with tempfile.TemporaryDirectory(prefix="dal stage ; ") as directory:
            root = Path(directory)
            output = root / "diagnostic-results"
            builds = diagnostic.layout(root)
            packages = {}
            for side, info in builds.items():
                info["source"].mkdir(parents=True, exist_ok=True)
                info["build"].mkdir(parents=True)
                (info["build"] / "CMakeCache.txt").write_text(
                    f"CMAKE_HOME_DIRECTORY:INTERNAL={info['source']}\n"
                    "CMAKE_BUILD_TYPE:STRING=Release\nDAL_BUILD_PYTHON:BOOL=ON\nBUILD_SHARED_LIBS:BOOL=OFF\n")
                binary = info["build"] / "dal-cpp/benchmarks/tape_perf/tape_perf"
                binary.parent.mkdir(parents=True)
                binary.write_text(f"#!{sys.executable}\n# Independent {side} fixture binary\nimport sys\n"
                                  "value=105 if 'benchmark-base' not in sys.argv[0] else 100\n"
                                  "print(f'Case  {value} ns  {value} ns  {value} ns  100')\n")
                binary.chmod(0o755)
                package = info["build"] / "dal-python"
                (package / "dal").mkdir(parents=True)
                (package / "dal/_dal.cpython-313-x86_64-linux-gnu.so").write_bytes(b"fixture")
                packages[side] = str(package)
            diagnostic.write_json(output / "selection.json", {"python_mode": "rebuild", "packages": packages})
            def prefix_pass(suite, package, destination):
                return [sys.executable, "-c", "print('numeric fixture preflight passed')"]
            with mock.patch.object(diagnostic, "source_identity", side_effect=lambda path, sha: {"sha": sha}), \
                    mock.patch.object(diagnostic, "prefix_command", side_effect=prefix_pass), \
                    mock.patch.object(diagnostic, "run_prefix", return_value=0):
                self.assertEqual(diagnostic.measure(root, output), 1)
            statuses = json.loads((output / "measurement-status.json").read_text())
            self.assertEqual(statuses, {"original_ci_exit_status": 1, "tape": 1, "python-prefix": 0})
            self.assertEqual(json.loads((output / "tape/results.json").read_text())["retry_benchmarks"], [])
            self.assertTrue((output / "binaries/base/tape_perf").is_file())
            self.assertTrue((output / "binaries/head/python-package.tar.gz").is_file())
            self.assertEqual(json.loads((output / "identities-before.json").read_text()),
                             json.loads((output / "identities-after.json").read_text()))


if __name__ == "__main__":
    unittest.main()

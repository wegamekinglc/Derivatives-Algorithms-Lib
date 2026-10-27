"""Execute the diagnostic setup entry with isolated package-manager fixtures."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest


SCRIPTS = Path(__file__).resolve().parents[1]
WORKFLOW = SCRIPTS.parent / "workflows/benchmarks.yml"
FIXTURE = Path(__file__).with_name("fixtures") / "dal469-profiler-404.txt"


@unittest.skipUnless(sys.platform == "linux", "requires Linux command fixtures")
class DiagnosticSetupTest(unittest.TestCase):
    def run_setup(self, root, **settings):
        commands = root / "bin"
        commands.mkdir(parents=True)
        (root / ".github").mkdir()
        (root / ".github/scripts").symlink_to(SCRIPTS, target_is_directory=True)
        stub = commands / "stub.py"
        stub.write_text(f"#!{sys.executable}\n" + textwrap.dedent('''
            import json, os, shutil, sys
            from pathlib import Path
            name, args = Path(sys.argv[0]).name, sys.argv[1:]
            root = Path(os.environ['FIXTURE_ROOT'])
            if name == 'sudo':
                os.execv(shutil.which(args[0]), args)
            with (root / 'trace.jsonl').open('a') as stream:
                stream.write(json.dumps([name, *args]) + '\\n')
            def status(key):
                sys.exit(int(os.environ.get(key, '0')))
            if name == 'apt-get':
                if args[0] == 'update':
                    print('Fixture apt index refreshed')
                    status('INDEX_STATUS')
                if 'linux-tools-generic' in args:
                    if os.environ.get('PROFILER_404') == '1':
                        print(Path(os.environ['APT_FIXTURE']).read_text())
                        sys.exit(100)
                elif 'gdb' in args:
                    status('GDB_INSTALL_STATUS')
                elif any(arg.startswith(('gcc-14', 'g++-14')) for arg in args):
                    if int(os.environ.get('COMPILER_INSTALL_STATUS', '0')) == 0:
                        version = '14.2.0' if any('=' in arg for arg in args) else '14.3.0'
                        (root / 'installed-version').write_text(os.environ.get('INSTALLED_VERSION', version))
                    status('COMPILER_INSTALL_STATUS')
                else:
                    status('REQUIRED_INSTALL_STATUS')
            elif name == 'apt-cache':
                if args[0] == 'madison':
                    for package in args[1:]:
                        version = '14.2.0-4ubuntu2~24.04.1' if os.environ.get('EXACT_AVAILABLE', '1') == '1' else '14.3.0-1'
                        print(f'{package} | {version} | fixture repository')
                else:
                    print('Fixture apt policy: gcc-14 and g++-14 installed 14.2.0-4ubuntu2~24.04.1')
            elif name in ('gcc-14', 'g++-14'):
                installed = root / 'installed-version'
                version = installed.read_text() if installed.exists() else os.environ.get('COMPILER_VERSION', '14.2.0')
                if version == 'missing':
                    sys.exit(127)
                print(version)
            elif name == 'perf' and os.environ.get('PROFILER_404') == '1':
                print('perf unavailable for fixture kernel')
                sys.exit(1)
            elif name == 'gdb':
                status('GDB_VERSION_STATUS')
            else:
                print(f'{name} fixture version')
        '''))
        stub.chmod(0o755)
        for name in ("sudo", "apt-get", "apt-cache", "gcc-14", "g++-14", "gdb", "perf", "make"):
            (commands / name).symlink_to(stub)
        job = WORKFLOW.read_text().split("\n  diagnostic:\n", 1)[1]
        step = job.split("- name: Set up diagnostic toolchain", 1)[1].split("      - uses:", 1)[0]
        script = textwrap.dedent(step.split("        run: |\n", 1)[1])
        result = subprocess.run(["bash", "-e", "-o", "pipefail", "-c", script], cwd=root,
                                env={**os.environ, "PATH": f"{commands}:{os.environ['PATH']}",
                                     "FIXTURE_ROOT": str(root), "APT_FIXTURE": str(FIXTURE),
                                     "GITHUB_EVENT_NAME": "workflow_dispatch", "DAL469_MODE": "dal-469",
                                     **{key: str(value) for key, value in settings.items()}},
                                capture_output=True, text=True, timeout=30)
        trace_file = root / "trace.jsonl"
        trace = [json.loads(line) for line in trace_file.read_text().splitlines()] if trace_file.exists() else []
        return result, trace, root / "diagnostic-results/setup"

    def test_installed_original_gcc_and_profiler_404_do_not_fallback_or_fail_setup(self):
        with tempfile.TemporaryDirectory(prefix="dal setup ; literal ") as directory:
            result, trace, output = self.run_setup(Path(directory), PROFILER_404=1)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertFalse(any(call[0] == "apt-get" and any(arg.startswith("gcc-14") for arg in call) for call in trace))
            status = json.loads((output / "status.json").read_text())
            self.assertEqual(status["toolchain"]["actual"], {"gcc-14": "14.2.0", "g++-14": "14.2.0"})
            self.assertEqual(status["toolchain"]["selection"], "installed-original")
            self.assertEqual(status["optional"]["perf"]["install_status"], 100)
            self.assertFalse(status["optional"]["perf"]["available"])
            self.assertEqual((output / "apt-install-perf.log").read_text().strip(), FIXTURE.read_text().strip())
            self.assertEqual(status["returncode"], 0)
            self.assertNotIn("Original GCC package unavailable", result.stdout)

    def test_index_refresh_precedes_installations_and_is_retained_for_upload(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(trace[0], ["apt-get", "update", "-o", "APT::Update::Error-Mode=any"])
            self.assertIn("index refreshed", (output / "apt-update.log").read_text())
            record = json.loads((output / "apt-update.command.json").read_text())
            self.assertEqual(record["returncode"], 0)
            self.assertEqual(record["argv"][0:3], ["sudo", "apt-get", "update"])
            status = json.loads((output / "status.json").read_text())
            self.assertEqual(status["required_status"], 0)
            self.assertEqual(status["original_ci_exit_status"], 1)
            job = WORKFLOW.read_text().split("\n  diagnostic:\n", 1)[1]
            upload = job.split("- name: Upload diagnostic evidence", 1)[1]
            self.assertIn("if: always()", upload)
            self.assertIn("path: diagnostic-results", upload)

    def test_failed_index_refresh_is_fatal_before_any_install(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory), INDEX_STATUS=100)
            self.assertEqual(result.returncode, 100)
            self.assertEqual(len(trace), 1)
            self.assertEqual(json.loads((output / "apt-update.command.json").read_text())["returncode"], 100)
            status = json.loads((output / "status.json").read_text())
            self.assertEqual(status["required_status"], 100)
            self.assertEqual(status["original_ci_exit_status"], 1)

    def test_required_tool_install_failure_keeps_exit_code_and_skips_profilers(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory), REQUIRED_INSTALL_STATUS=100)
            self.assertEqual(result.returncode, 100)
            self.assertEqual([call for call in trace if call[:2] == ["apt-get", "install"]],
                             [["apt-get", "install", "-y", "make", "time"]])
            self.assertEqual(json.loads((output / "apt-install-required.command.json").read_text())["returncode"], 100)
            status = json.loads((output / "status.json").read_text())
            self.assertEqual(status["returncode"], 100)
            self.assertEqual(status["toolchain"]["selection"], "installed-original")
            self.assertNotIn("optional", status)

    def test_missing_compilers_install_available_original_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory), COMPILER_VERSION="missing")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(["apt-get", "install", "-y", "gcc-14=14.2.0-4ubuntu2~24.04.1",
                           "g++-14=14.2.0-4ubuntu2~24.04.1"], trace)
            toolchain = json.loads((output / "status.json").read_text())["toolchain"]
            self.assertEqual(toolchain["before"], {"gcc-14": None, "g++-14": None})
            self.assertEqual(toolchain["selection"], "install-original")
            self.assertEqual(toolchain["differences_from_original"], {})

    def test_unavailable_original_version_selects_generic_gcc14_and_records_difference(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory), COMPILER_VERSION="missing", EXACT_AVAILABLE=0)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(["apt-get", "install", "-y", "gcc-14", "g++-14"], trace)
            toolchain = json.loads((output / "status.json").read_text())["toolchain"]
            self.assertEqual(toolchain["selection"], "available-gcc-14")
            self.assertIn("unavailable in refreshed index", toolchain["fallback_reason"])
            self.assertEqual(toolchain["actual"], {"gcc-14": "14.3.0", "g++-14": "14.3.0"})
            self.assertEqual(toolchain["differences_from_original"]["gcc-14"],
                             {"expected": "14.2.0", "actual": "14.3.0"})

    def test_compiler_install_failure_is_fatal_without_generic_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory), COMPILER_VERSION="missing", COMPILER_INSTALL_STATUS=100)
            self.assertEqual(result.returncode, 100)
            installs = [call for call in trace if call[:2] == ["apt-get", "install"]]
            self.assertEqual(len(installs), 1)
            self.assertIn("gcc-14=14.2.0-4ubuntu2~24.04.1", installs[0])
            self.assertEqual(json.loads((output / "apt-install-compilers.command.json").read_text())["returncode"], 100)
            self.assertEqual(json.loads((output / "status.json").read_text())["returncode"], 100)

    def test_invalid_actual_compiler_version_is_fatal(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory), COMPILER_VERSION="missing", INSTALLED_VERSION="13.3.0")
            self.assertEqual(result.returncode, 1)
            self.assertIn("verification failed", result.stderr)
            status = json.loads((output / "status.json").read_text())
            self.assertEqual(status["toolchain"]["actual"]["g++-14"], "13.3.0")
            self.assertEqual(status["required_status"], 1)
            self.assertFalse(any("make" in call for call in trace))

    def test_optional_gdb_failure_does_not_skip_perf_or_clear_required_status(self):
        with tempfile.TemporaryDirectory() as directory:
            result, trace, output = self.run_setup(Path(directory), GDB_INSTALL_STATUS=100, GDB_VERSION_STATUS=127)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(["apt-get", "install", "-y", "linux-tools-generic"], trace)
            status = json.loads((output / "status.json").read_text())
            self.assertEqual(status["required_status"], 0)
            self.assertEqual(status["optional"]["gdb"], {"install_status": 100, "version_status": 127, "available": False})
            self.assertTrue(status["optional"]["perf"]["available"])

    def test_nonmanual_or_unrecognized_mode_never_invokes_package_manager(self):
        for event, mode in (("schedule", "dal-469"), ("workflow_dispatch", "scheduled"),
                            ("workflow_dispatch", "dal-469; touch injected")):
            with self.subTest(event=event, mode=mode), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                result, trace, output = self.run_setup(root, GITHUB_EVENT_NAME=event, DAL469_MODE=mode)
                self.assertEqual(result.returncode, 1)
                self.assertEqual(trace, [])
                self.assertIn("requires manual dal-469 mode", result.stderr)
                self.assertEqual(json.loads((output / "status.json").read_text())["returncode"], 1)
                self.assertFalse((root / "injected").exists())


if __name__ == "__main__":
    unittest.main()

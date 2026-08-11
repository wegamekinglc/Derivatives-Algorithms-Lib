"""Executable Windows tests for the PowerShell Python helpers."""

import base64
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
PWSH = shutil.which("pwsh")
DOTNET = shutil.which("dotnet")


PROCESS_DOUBLE_SOURCE = r"""
using System;
using System.Diagnostics;
using System.IO;
using System.Text;

public static class ProcessDouble {
    private static void Log(string executable, string[] arguments) {
        string path = Environment.GetEnvironmentVariable("PROCESS_DOUBLE_LOG");
        string[] encoded = new string[arguments.Length];
        for (int index = 0; index < arguments.Length; ++index) {
            encoded[index] = Convert.ToBase64String(Encoding.UTF8.GetBytes(arguments[index]));
        }
        File.AppendAllText(path, executable + "\t" + String.Join("\t", encoded) + Environment.NewLine);
    }

    private static void CreateEnvironment(string path) {
        string scripts = Path.Combine(Path.GetFullPath(path), "Scripts");
        Directory.CreateDirectory(scripts);
        string executable = Process.GetCurrentProcess().MainModule.FileName;
        File.Copy(executable, Path.Combine(scripts, "python.exe"), true);
        string assemblyDirectory = Path.GetDirectoryName(typeof(ProcessDouble).Assembly.Location);
        foreach (string suffix in new string[] { ".dll", ".deps.json", ".runtimeconfig.json" }) {
            string source = Path.Combine(assemblyDirectory, "process_double" + suffix);
            if (File.Exists(source)) {
                File.Copy(source, Path.Combine(scripts, Path.GetFileName(source)), true);
            }
        }
        File.WriteAllText(
            Path.Combine(scripts, "Activate.ps1"),
            "$env:PATH = \"$PSScriptRoot;$env:PATH\"\r\n"
        );
    }

    public static int Main(string[] arguments) {
        string executablePath = Process.GetCurrentProcess().MainModule.FileName;
        string executable = Path.GetFileNameWithoutExtension(executablePath).ToLowerInvariant();
        Log(executable, arguments);

        if (executable == "uv" && arguments.Length > 0 && arguments[0] == "venv") {
            string failure = Environment.GetEnvironmentVariable("PROCESS_DOUBLE_UV_VENV_EXIT");
            if (!String.IsNullOrEmpty(failure)) {
                return Int32.Parse(failure);
            }
            CreateEnvironment(arguments[1]);
        }
        if (executable == "uv" && arguments.Length > 0 && arguments[0] == "build") {
            Directory.CreateDirectory("dist");
            File.WriteAllText(
                Path.Combine("dist", "dal_python-test-cp39-cp39-win_amd64.whl"),
                "process double"
            );
        }
        if (
            executable == "python"
            && arguments.Length > 0
            && arguments[0].EndsWith("python_compat.py", StringComparison.OrdinalIgnoreCase)
        ) {
            string failure = Environment.GetEnvironmentVariable("PROCESS_DOUBLE_COMPAT_EXIT");
            if (!String.IsNullOrEmpty(failure)) {
                Console.Error.WriteLine(
                    "process double rejected reused environment: requested 3.9, observed CPython 3.13; rerun with -Clean"
                );
                return Int32.Parse(failure);
            }
        }
        return 0;
    }
}
"""


@unittest.skipUnless(
    os.name == "nt" and PWSH and DOTNET, "requires Windows pwsh and .NET SDK"
)
class PythonPowerShellHelpersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._compiler_directory = tempfile.TemporaryDirectory()
        directory = Path(cls._compiler_directory.name)
        project = directory / "process-double"
        project.mkdir()
        source = project / "Program.cs"
        project_file = project / "process_double.csproj"
        cls.process_double_directory = directory / "output"
        source.write_text(PROCESS_DOUBLE_SOURCE, encoding="utf-8")
        project_file.write_text(
            "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
            "  <PropertyGroup>\n"
            "    <OutputType>Exe</OutputType>\n"
            "    <TargetFramework>net8.0</TargetFramework>\n"
            "    <ImplicitUsings>disable</ImplicitUsings>\n"
            "    <Nullable>disable</Nullable>\n"
            "    <UseAppHost>true</UseAppHost>\n"
            "    <AssemblyName>process_double</AssemblyName>\n"
            "  </PropertyGroup>\n"
            "</Project>\n",
            encoding="utf-8",
        )
        subprocess.run(
            (
                DOTNET,
                "publish",
                str(project_file),
                "-c",
                "Release",
                "-o",
                str(cls.process_double_directory),
                "--nologo",
            ),
            check=True,
        )
        cls.process_double = cls.process_double_directory / "process_double.exe"

    @classmethod
    def tearDownClass(cls):
        cls._compiler_directory.cleanup()

    def helper_tree(self, directory: Path):
        root = directory / "repo"
        package = root / "dal-python"
        scripts = package / "scripts"
        scripts.mkdir(parents=True)
        for name in ("run_tests.ps1", "build_wheel.ps1"):
            shutil.copy2(ROOT / "dal-python" / name, package / name)
        (scripts / "python_compat.py").write_text("# process-double target\n", encoding="utf-8")

        install = root / "stage"
        libraries = install / "lib"
        libraries.mkdir(parents=True)
        (libraries / "dal_public.lib").touch()
        (libraries / "dal_cpp.lib").touch()

        fake_bin = root / "fake-bin"
        self.install_process_double(fake_bin, "uv.exe")
        log = root / "process-double.log"
        return package, install, fake_bin, log

    def install_process_double(self, directory: Path, executable_name: str):
        directory.mkdir(parents=True)
        for source in self.process_double_directory.iterdir():
            if source.is_file():
                shutil.copy2(source, directory / source.name)
        shutil.copy2(self.process_double, directory / executable_name)

    def environment(self, fake_bin: Path, log: Path, **overrides):
        environment = os.environ.copy()
        environment.update(
            {
                "PATH": f"{fake_bin}{os.pathsep}{environment['PATH']}",
                "PROCESS_DOUBLE_LOG": str(log),
            }
        )
        environment.update(overrides)
        return environment

    def run_helper(self, package: Path, name: str, arguments, environment):
        return subprocess.run(
            (
                PWSH,
                "-NoLogo",
                "-NoProfile",
                "-File",
                str(package / name),
                *arguments,
            ),
            cwd=package,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )

    def calls(self, log: Path):
        calls = []
        if not log.exists():
            return calls
        for line in log.read_text(encoding="utf-8").splitlines():
            executable, *encoded = line.split("\t")
            arguments = [
                base64.b64decode(value).decode("utf-8") for value in encoded if value
            ]
            calls.append((executable, arguments))
        return calls

    def create_reused_environment(self, package: Path):
        scripts = package / ".venv" / "Scripts"
        self.install_process_double(scripts, "python.exe")
        (scripts / "Activate.ps1").write_text(
            '$env:PATH = "$PSScriptRoot;$env:PATH"\n', encoding="utf-8"
        )
        sentinel = scripts.parent / "sentinel"
        sentinel.write_text("preserve", encoding="utf-8")
        return sentinel

    def test_helpers_pass_each_exact_and_omitted_selector_to_provisioning(self):
        for name in ("run_tests.ps1", "build_wheel.ps1"):
            for selector in ("3.9", "3.10", "3.11", "3.12", "3.13", None):
                with self.subTest(name=name, selector=selector), tempfile.TemporaryDirectory() as tmp:
                    package, install, fake_bin, log = self.helper_tree(Path(tmp))
                    arguments = ["-Clean", "-DalInstallPrefix", str(install)]
                    if selector is not None:
                        arguments.extend(("-Python", selector))
                    result = self.run_helper(
                        package,
                        name,
                        arguments,
                        self.environment(fake_bin, log),
                    )

                    self.assertEqual(result.returncode, 0, result.stdout)
                    venv_calls = [
                        arguments
                        for executable, arguments in self.calls(log)
                        if executable == "uv" and arguments[:1] == ["venv"]
                    ]
                    expected = selector or ">=3.9,<3.14"
                    self.assertEqual(len(venv_calls), 1)
                    self.assertEqual(venv_calls[0][-2:], ["--python", expected])

    def test_run_tests_preserves_pytest_argument_vector_after_selector(self):
        with tempfile.TemporaryDirectory() as tmp:
            package, install, fake_bin, log = self.helper_tree(Path(tmp))
            result = self.run_helper(
                package,
                "run_tests.ps1",
                (
                    "-Python",
                    "3.9",
                    "-DalInstallPrefix",
                    str(install),
                    "--maxfail=1",
                    "-k",
                    "date or calendar",
                ),
                self.environment(fake_bin, log),
            )

            self.assertEqual(result.returncode, 0, result.stdout)
            pytest_calls = [
                arguments
                for executable, arguments in self.calls(log)
                if executable == "python" and arguments[:2] == ["-m", "pytest"]
            ]
            self.assertEqual(
                pytest_calls,
                [["-m", "pytest", "tests/", "-v", "--maxfail=1", "-k", "date or calendar"]],
            )

    def test_helpers_reject_invalid_selectors_before_provisioning(self):
        for name in ("run_tests.ps1", "build_wheel.ps1"):
            for selector in ("", "3.14", "cpython3.9"):
                with self.subTest(name=name, selector=selector), tempfile.TemporaryDirectory() as tmp:
                    package, install, fake_bin, log = self.helper_tree(Path(tmp))
                    result = self.run_helper(
                        package,
                        name,
                        ("-Python", selector, "-DalInstallPrefix", str(install)),
                        self.environment(fake_bin, log),
                    )

                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("unsupported value", result.stdout)
                    self.assertEqual(self.calls(log), [])

    def test_helpers_fail_without_uv_or_requested_interpreter_fallback(self):
        for name in ("run_tests.ps1", "build_wheel.ps1"):
            with self.subTest(name=name, failure="uv"), tempfile.TemporaryDirectory() as tmp:
                package, install, fake_bin, log = self.helper_tree(Path(tmp))
                environment = self.environment(fake_bin, log)
                environment["PATH"] = str(Path(os.environ["SystemRoot"]) / "System32")
                result = self.run_helper(
                    package,
                    name,
                    ("-Python", "3.9", "-DalInstallPrefix", str(install)),
                    environment,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("uv is", result.stdout)
                self.assertEqual(self.calls(log), [])

            with self.subTest(name=name, failure="interpreter"), tempfile.TemporaryDirectory() as tmp:
                package, install, fake_bin, log = self.helper_tree(Path(tmp))
                result = self.run_helper(
                    package,
                    name,
                    ("-Python", "3.9", "-DalInstallPrefix", str(install)),
                    self.environment(fake_bin, log, PROCESS_DOUBLE_UV_VENV_EXIT="42"),
                )
                self.assertNotEqual(result.returncode, 0)
                venv_calls = [
                    arguments
                    for executable, arguments in self.calls(log)
                    if executable == "uv" and arguments[:1] == ["venv"]
                ]
                self.assertEqual(len(venv_calls), 1)
                self.assertEqual(venv_calls[0][-2:], ["--python", "3.9"])

    def test_helpers_reuse_matching_environment_without_recreation(self):
        for name in ("run_tests.ps1", "build_wheel.ps1"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                package, install, fake_bin, log = self.helper_tree(Path(tmp))
                sentinel = self.create_reused_environment(package)
                result = self.run_helper(
                    package,
                    name,
                    ("-Python", "3.9", "-DalInstallPrefix", str(install)),
                    self.environment(fake_bin, log),
                )

                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")
                self.assertFalse(
                    any(
                        executable == "uv" and arguments[:1] == ["venv"]
                        for executable, arguments in self.calls(log)
                    )
                )

    def test_helpers_stop_before_mutation_when_reused_environment_is_rejected(self):
        for name in ("run_tests.ps1", "build_wheel.ps1"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                package, install, fake_bin, log = self.helper_tree(Path(tmp))
                sentinel = self.create_reused_environment(package)
                result = self.run_helper(
                    package,
                    name,
                    ("-Python", "3.9", "-DalInstallPrefix", str(install)),
                    self.environment(fake_bin, log, PROCESS_DOUBLE_COMPAT_EXIT="23"),
                )

                self.assertEqual(result.returncode, 23, result.stdout)
                self.assertIn("requested 3.9", result.stdout)
                self.assertIn("observed CPython 3.13", result.stdout)
                self.assertIn("-Clean", result.stdout)
                self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")
                self.assertFalse(
                    any(executable == "uv" for executable, _ in self.calls(log))
                )


if __name__ == "__main__":
    unittest.main()

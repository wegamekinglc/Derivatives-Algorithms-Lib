"""Tests for shared local Python helper validation."""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SCRIPT = (
    Path(__file__).resolve().parents[3]
    / "dal-python"
    / "scripts"
    / "python_compat.py"
)
SPEC = importlib.util.spec_from_file_location("python_compat", SCRIPT)
PYTHON_COMPAT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PYTHON_COMPAT)


class PythonHelpersTest(unittest.TestCase):
    def test_accepts_each_supported_cpython_minor(self):
        for minor in ("3.9", "3.10", "3.11", "3.12", "3.13"):
            major, minor_number = (int(part) for part in minor.split("."))
            with self.subTest(minor=minor):
                observed = PYTHON_COMPAT.validate_interpreter(
                    "dal-python/run_tests.sh",
                    Path(".venv"),
                    minor,
                    "CPython",
                    (major, minor_number),
                    "rerun with --clean",
                )
                self.assertEqual(observed, f"CPython {minor}")

        self.assertEqual(
            PYTHON_COMPAT.validate_interpreter(
                "dal-python/run_tests.sh",
                Path(".venv"),
                None,
                "CPython",
                (3, 12),
                "replace the environment",
            ),
            "CPython 3.12",
        )

    def test_build_linux_rejects_unsupported_selector_before_build(self):
        for script in (
            "build_linux.sh",
            "dal-python/build_sdist.sh",
            "dal-python/build_wheel.sh",
            "dal-python/run_tests.sh",
        ):
            with self.subTest(script=script):
                result = subprocess.run(
                    ("bash", script, "--python", "3.14"),
                    cwd=SCRIPT.parents[2],
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    check=False,
                )

                self.assertNotEqual(result.returncode, 0)
                self.assertIn("unsupported value '3.14'", result.stdout)

    def test_rejected_reused_environment_is_non_destructive_and_actionable(self):
        with tempfile.TemporaryDirectory() as tmp:
            environment = Path(tmp) / ".venv"
            environment.mkdir()
            sentinel = environment / "sentinel"
            sentinel.write_text("preserve", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "dal-python/run_tests.sh") as raised:
                PYTHON_COMPAT.validate_interpreter(
                    "dal-python/run_tests.sh",
                    environment,
                    "3.9",
                    "CPython",
                    (3, 13),
                    f"remove or replace {environment}, then rerun",
                )

            message = str(raised.exception)
            self.assertIn(str(environment), message)
            self.assertIn("CPython 3.13", message)
            self.assertIn("requested 3.9", message)
            self.assertIn("remove or replace", message)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")

    def test_rejects_non_cpython_and_out_of_range_interpreters(self):
        for implementation, version, expected in (
            ("PyPy", (3, 9), "requires CPython"),
            ("CPython", (3, 8), ">=3.9,<3.14"),
            ("CPython", (3, 14), ">=3.9,<3.14"),
        ):
            with self.subTest(implementation=implementation, version=version):
                with self.assertRaisesRegex(ValueError, expected):
                    PYTHON_COMPAT.validate_interpreter(
                        "entry",
                        Path(".venv"),
                        None,
                        implementation,
                        version,
                        "replace the environment",
                    )

    def test_all_six_helpers_validate_before_mutation(self):
        root = SCRIPT.parents[2]
        helpers = (
            root / "build_linux.sh",
            root / "dal-python/build_sdist.sh",
            root / "dal-python/build_wheel.sh",
            root / "dal-python/build_wheel.ps1",
            root / "dal-python/run_tests.sh",
            root / "dal-python/run_tests.ps1",
        )
        for helper in helpers:
            with self.subTest(helper=helper.name):
                text = helper.read_text(encoding="utf-8")
                selector = "-Python" if helper.suffix == ".ps1" else "--python"
                self.assertIn(selector, text)
                for minor in ("3.9", "3.10", "3.11", "3.12", "3.13"):
                    self.assertIn(minor, text)
                self.assertLess(text.index("python_compat.py"), text.index("uv pip install"))

    def test_test_wrappers_preserve_remaining_argument_vectors(self):
        root = SCRIPT.parents[2]
        posix = (root / "dal-python/run_tests.sh").read_text(encoding="utf-8")
        powershell = (root / "dal-python/run_tests.ps1").read_text(encoding="utf-8")

        self.assertIn('PYTEST_ARGS+=("$1")', posix)
        self.assertIn('"${PYTEST_ARGS[@]}"', posix)
        self.assertIn("ValueFromRemainingArguments", powershell)
        self.assertIn("$EffectivePytestArgs += $PytestArgs", powershell)
        self.assertIn("python -m pytest @EffectivePytestArgs", powershell)

    def test_package_helpers_fail_clearly_without_uv(self):
        root = SCRIPT.parents[2]
        for relative in (
            "dal-python/build_sdist.sh",
            "dal-python/build_wheel.sh",
            "dal-python/run_tests.sh",
        ):
            with self.subTest(relative=relative):
                text = (root / relative).read_text(encoding="utf-8")
                self.assertIn("command -v uv", text)
                self.assertIn("uv is", text)
        for relative in ("dal-python/build_wheel.ps1", "dal-python/run_tests.ps1"):
            with self.subTest(relative=relative):
                text = (root / relative).read_text(encoding="utf-8")
                self.assertIn("Get-Command uv", text)
                self.assertIn("uv is", text)

    def test_posix_helpers_pass_exact_selectors_to_uv(self):
        source_root = SCRIPT.parents[2]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "repo"
            package = root / "dal-python"
            scripts = package / "scripts"
            scripts.mkdir(parents=True)
            shutil.copy2(source_root / "build_linux.sh", root / "build_linux.sh")
            for relative in ("build_sdist.sh", "build_wheel.sh", "run_tests.sh"):
                shutil.copy2(source_root / "dal-python" / relative, package / relative)
            shutil.copy2(SCRIPT, scripts / SCRIPT.name)

            install = root / "stage"
            config = install / "lib" / "cmake" / "dal-public" / "dal-publicConfig.cmake"
            config.parent.mkdir(parents=True)
            config.touch()
            fake_bin = root / "fake-bin"
            fake_bin.mkdir()
            log = root / "uv.log"
            uv = fake_bin / "uv"
            uv.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$UV_LOG"\nexit 42\n', encoding="utf-8")
            uv.chmod(0o755)
            environment = os.environ.copy()
            environment.update(
                {
                    "PATH": f"{fake_bin}:/usr/bin:/bin",
                    "UV_LOG": str(log),
                    "DAL_INSTALL_PREFIX": str(install),
                }
            )
            helpers = (
                (root, "build_linux.sh", ("--python",)),
                (package, "build_sdist.sh", ("--python",)),
                (package, "build_wheel.sh", ("--python",)),
                (package, "run_tests.sh", ("--python",)),
            )
            for minor in ("3.9", "3.10", "3.11", "3.12", "3.13"):
                for cwd, script, prefix in helpers:
                    with self.subTest(minor=minor, script=script):
                        log.unlink(missing_ok=True)
                        result = subprocess.run(
                            ("bash", script, *prefix, minor),
                            cwd=cwd,
                            env=environment,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True,
                            check=False,
                        )
                        self.assertEqual(result.returncode, 42, result.stdout)
                        self.assertIn(f"--python {minor}", log.read_text(encoding="utf-8"))

    def test_posix_helpers_preserve_rejected_reused_environment(self):
        source_root = SCRIPT.parents[2]
        # The reused environment wraps the host interpreter, so request a
        # supported minor that always differs from the host's.
        requested = "3.10" if "%d.%d" % os.sys.version_info[:2] == "3.9" else "3.9"
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "repo"
            package = root / "dal-python"
            scripts = package / "scripts"
            scripts.mkdir(parents=True)
            shutil.copy2(source_root / "build_linux.sh", root / "build_linux.sh")
            for relative in ("build_sdist.sh", "build_wheel.sh", "run_tests.sh"):
                shutil.copy2(source_root / "dal-python" / relative, package / relative)
            shutil.copy2(SCRIPT, scripts / SCRIPT.name)
            install = root / "stage"
            config = install / "lib" / "cmake" / "dal-public" / "dal-publicConfig.cmake"
            config.parent.mkdir(parents=True)
            config.touch()

            venv_python = package / ".venv" / "bin" / "python"
            venv_python.parent.mkdir(parents=True)
            venv_python.symlink_to(Path(os.sys.executable))
            sentinel = venv_python.parent.parent / "sentinel"
            sentinel.write_text("preserve", encoding="utf-8")
            fake_bin = root / "fake-bin"
            fake_bin.mkdir()
            uv_log = root / "uv.log"
            uv = fake_bin / "uv"
            uv.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$UV_LOG"\n', encoding="utf-8")
            uv.chmod(0o755)
            environment = os.environ.copy()
            environment.update(
                {
                    "PATH": f"{fake_bin}:/usr/bin:/bin",
                    "UV_LOG": str(uv_log),
                    "DAL_INSTALL_PREFIX": str(install),
                }
            )
            helpers = (
                (root, "build_linux.sh"),
                (package, "build_sdist.sh"),
                (package, "build_wheel.sh"),
                (package, "run_tests.sh"),
            )
            for cwd, script in helpers:
                with self.subTest(script=script):
                    result = subprocess.run(
                        ("bash", script, "--python", requested),
                        cwd=cwd,
                        env=environment,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        check=False,
                    )
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(f"requested {requested}", result.stdout)
                    self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")
                    self.assertFalse(uv_log.exists())

    def test_posix_test_wrapper_forwards_argument_vector_in_order(self):
        source_root = SCRIPT.parents[2]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "repo"
            package = root / "dal-python"
            scripts = package / "scripts"
            scripts.mkdir(parents=True)
            shutil.copy2(source_root / "dal-python/run_tests.sh", package / "run_tests.sh")
            shutil.copy2(SCRIPT, scripts / SCRIPT.name)
            config = root / "stage/lib/cmake/dal-public/dal-publicConfig.cmake"
            config.parent.mkdir(parents=True)
            config.touch()
            fake_bin = root / "fake-bin"
            fake_bin.mkdir()
            uv = fake_bin / "uv"
            uv.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            uv.chmod(0o755)

            venv_bin = package / ".venv/bin"
            venv_bin.mkdir(parents=True)
            (venv_bin / "activate").touch()
            python_log = root / "python.log"
            python = venv_bin / "python"
            python.write_text(
                "#!/bin/sh\n"
                'if [ "$1" = "$COMPAT_SCRIPT" ]; then exit 0; fi\n'
                'if [ "$1" = "-c" ]; then exit 0; fi\n'
                'if [ "$1" = "--version" ]; then exit 0; fi\n'
                'for arg in "$@"; do printf "%s\\n" "$arg" >> "$PYTHON_LOG"; done\n'
                "exit 0\n",
                encoding="utf-8",
            )
            python.chmod(0o755)
            environment = os.environ.copy()
            environment.update(
                {
                    "PATH": f"{venv_bin}:{fake_bin}:/usr/bin:/bin",
                    "DAL_INSTALL_PREFIX": str(root / "stage"),
                    "COMPAT_SCRIPT": str(scripts / SCRIPT.name),
                    "PYTHON_LOG": str(python_log),
                }
            )

            result = subprocess.run(
                (
                    "bash",
                    "run_tests.sh",
                    "--python",
                    "3.13",
                    "--maxfail=1",
                    "-k",
                    "date or calendar",
                ),
                cwd=package,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(
                python_log.read_text(encoding="utf-8").splitlines(),
                ["-m", "pytest", "tests/", "-v", "--maxfail=1", "-k", "date or calendar"],
            )


if __name__ == "__main__":
    unittest.main()

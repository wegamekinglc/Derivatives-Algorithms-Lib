"""Tests for the isolated dal-python wheel smoke helper."""

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import call, patch


SCRIPT = (
    Path(__file__).resolve().parents[3]
    / "dal-python"
    / "scripts"
    / "smoke_installed_wheel.py"
)


def load_smoke():
    spec = importlib.util.spec_from_file_location("smoke_installed_wheel", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PythonSmokeTest(unittest.TestCase):
    def test_rejects_wrong_interpreter_before_environment_creation(self):
        smoke = load_smoke()

        for implementation, version in (("CPython", (3, 10)), ("PyPy", (3, 9))):
            with self.subTest(implementation=implementation, version=version):
                with self.assertRaisesRegex(ValueError, "requires CPython 3.9"):
                    smoke.validate_smoke_interpreter(implementation, version)

    def test_rejects_nonisolated_or_user_site_runtime(self):
        smoke = load_smoke()

        for isolated, user_site in ((False, False), (True, True)):
            with self.subTest(isolated=isolated, user_site=user_site):
                with self.assertRaisesRegex(ValueError, "isolated mode|user site-packages"):
                    smoke.validate_runtime_isolation(isolated, user_site, {})

    def test_rejects_version_and_extension_suffix_drift(self):
        smoke = load_smoke()

        with self.assertRaisesRegex(ValueError, "does not equal"):
            smoke.validate_versions("2026.8.11", "2026.8.12")
        with self.assertRaisesRegex(ValueError, "extension suffix"):
            smoke.validate_native_suffix(Path("dal/_dal.py"), (".so", ".pyd"))

    def test_rejects_nonempty_workdir_and_excluded_sys_path(self):
        smoke = load_smoke()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            work_dir = root / "work"
            work_dir.mkdir()
            (work_dir / "source.py").touch()
            excluded = root / "repository"
            excluded.mkdir()

            with self.assertRaisesRegex(ValueError, "not empty"):
                smoke.validate_work_directory(work_dir, work_dir)
            with self.assertRaisesRegex(ValueError, "excluded root"):
                smoke.validate_sys_path((excluded / "dal-python",), (excluded,))

    def test_rejects_reused_source_resolved_and_dependency_rich_environments(self):
        smoke = load_smoke()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            existing = root / "existing"
            existing.mkdir()
            site_packages = root / "environment" / "site-packages"
            site_packages.mkdir(parents=True)
            source = root / "repository" / "dal-python" / "src" / "dal" / "__init__.py"
            source.parent.mkdir(parents=True)
            source.touch()

            with self.assertRaisesRegex(ValueError, "already exists"):
                smoke.require_new_path(existing, "target environment")
            with self.assertRaisesRegex(ValueError, "outside fresh site-packages"):
                smoke.validate_installed_path(
                    "dal.__file__", source, (site_packages,), (root / "repository",)
                )
            with self.assertRaisesRegex(ValueError, "unexpected third-party"):
                smoke.validate_distribution_inventory(("dal-python", "pip", "pytest"))

    def test_rejects_python_path_or_python_home_leakage(self):
        smoke = load_smoke()

        for variable in ("PYTHONPATH", "PYTHONHOME"):
            with (
                self.subTest(variable=variable),
                self.assertRaisesRegex(ValueError, variable),
            ):
                smoke.validate_runtime_isolation(True, False, {variable: "/source"})

    def test_environment_commands_use_resolved_executable_and_argument_vectors(self):
        smoke = load_smoke()
        with tempfile.TemporaryDirectory() as tmp:
            environment = Path(tmp) / "environment"
            python = environment / "bin" / "python"
            wheel = Path(tmp) / "dal_python-2026.8.11-cp39-cp39-linux_x86_64.whl"
            self.assertEqual(smoke.executable_path(python), python)
            with (
                patch.object(smoke.shutil, "which", return_value="/opt/uv/bin/uv"),
                patch.object(smoke, "run_process") as run,
            ):
                smoke.create_environment(environment, Path("/opt/python/bin/python"))
                smoke.install_wheel(python, wheel)

            self.assertEqual(
                run.call_args_list,
                [
                    call(
                        Path("/opt/uv/bin/uv"),
                        (
                            "venv",
                            "--no-project",
                            "--python",
                            "/opt/python/bin/python",
                            str(environment),
                        ),
                    ),
                    call(
                        Path("/opt/uv/bin/uv"),
                        (
                            "pip",
                            "install",
                            "--python",
                            str(python),
                            "--no-config",
                            str(wheel),
                        ),
                    ),
                ],
            )


if __name__ == "__main__":
    unittest.main()

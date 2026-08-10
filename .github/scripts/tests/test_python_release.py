"""Tests for the dal-python binary release verifier."""

import importlib.util
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch
from zipfile import ZipFile


SCRIPT = Path(__file__).resolve().parents[3] / "dal-python" / "scripts" / "verify_release.py"
SPEC = importlib.util.spec_from_file_location("verify_release", SCRIPT)
VERIFY_RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY_RELEASE)

BUILD_SCRIPT = SCRIPT.with_name("build_native.py")
BUILD_SPEC = importlib.util.spec_from_file_location("build_native", BUILD_SCRIPT)
BUILD_NATIVE = importlib.util.module_from_spec(BUILD_SPEC)
BUILD_SPEC.loader.exec_module(BUILD_NATIVE)


class PythonReleaseTest(unittest.TestCase):
    def write_wheel(self, directory: Path, python_tag: str, platform_tag: str) -> Path:
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        extension = "pyd" if platform_tag == "win_amd64" else "so"
        wheel = directory / (
            f"dal_python-{version}-{python_tag}-{python_tag}-{platform_tag}.whl"
        )
        dist_info = f"dal_python-{version}.dist-info"
        with ZipFile(wheel, "w") as archive:
            archive.writestr(f"dal/_dal.{python_tag}.{extension}", b"native")
            archive.writestr(
                f"{dist_info}/METADATA",
                "\n".join(
                    (
                        "Metadata-Version: 2.4",
                        "Name: dal-python",
                        f"Version: {version}",
                        "License-Expression: MIT",
                        f"Requires-Python: {requires_python}",
                        "",
                        "DAL package description",
                    )
                ),
            )
            archive.writestr(
                f"{dist_info}/WHEEL",
                "\n".join(
                    (
                        "Wheel-Version: 1.0",
                        "Root-Is-Purelib: false",
                        f"Tag: {python_tag}-{python_tag}-{platform_tag}",
                        "",
                    )
                ),
            )
        return wheel

    def test_accepts_complete_platform_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            version, _, _ = VERIFY_RELEASE.project_configuration()
            self.write_wheel(directory, "cp313", "manylinux_2_28_x86_64")
            self.write_wheel(directory, "cp313", "win_amd64")

            manifest = VERIFY_RELEASE.validate_release(
                directory,
                expected_python_tags=("cp313",),
                tag=f"dal-python-v{version}",
            )

            self.assertEqual(len(manifest), 2)

    def test_rejects_raw_linux_platform_tag(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            version, requires_python, _ = VERIFY_RELEASE.project_configuration()
            wheel = self.write_wheel(directory, "cp313", "linux_x86_64")

            with self.assertRaisesRegex(ValueError, "unrepaired wheel platform"):
                VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_incomplete_platform_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            self.write_wheel(directory, "cp313", "manylinux_2_28_x86_64")

            with self.assertRaisesRegex(ValueError, "wheel matrix mismatch"):
                VERIFY_RELEASE.validate_release(directory, expected_python_tags=("cp313",))

    def test_rejects_tag_version_drift(self):
        version, _, _ = VERIFY_RELEASE.project_configuration()
        with self.assertRaisesRegex(ValueError, f"must equal {re.escape(f'dal-python-v{version}')}"):
            VERIFY_RELEASE.validate_versions("dal-python-v0")

    def test_finds_native_config_in_lib_and_lib64(self):
        relative = Path("cmake") / "dal-public" / "dal-publicConfig.cmake"
        for lib_dir in ("lib", "lib64"):
            with self.subTest(lib_dir=lib_dir), tempfile.TemporaryDirectory() as tmp:
                install_dir = Path(tmp)
                config = install_dir / lib_dir / relative
                config.parent.mkdir(parents=True)
                config.touch()

                self.assertEqual(BUILD_NATIVE.find_public_config(install_dir), config)

    def test_windows_native_build_selects_msvc_release(self):
        generator, build_config = BUILD_NATIVE.generator_configuration(is_windows=True)

        self.assertIn("Visual Studio 17 2022", generator)
        self.assertIn("x64", generator)
        self.assertEqual(build_config, ["--config", "Release"])

    def test_cmake_executable_is_resolved_to_absolute_path(self):
        with patch.object(BUILD_NATIVE, "which", return_value="/opt/cmake/bin/cmake"):
            self.assertEqual(
                BUILD_NATIVE.cmake_executable(), Path("/opt/cmake/bin/cmake")
            )

    def test_cmake_executable_rejects_unexpected_name(self):
        with patch.object(BUILD_NATIVE, "which", return_value="/tmp/not-cmake"):
            with self.assertRaisesRegex(RuntimeError, "unexpected cmake executable"):
                BUILD_NATIVE.cmake_executable()

    def test_run_cmake_uses_resolved_executable_without_shell(self):
        executable = Path("/opt/cmake/bin/cmake")
        with patch.object(
            BUILD_NATIVE, "cmake_executable", return_value=executable
        ), patch.object(BUILD_NATIVE.subprocess, "run") as run:
            BUILD_NATIVE.run_cmake(["--version"])

            run.assert_called_once_with(
                [str(executable), "--version"], check=True, shell=False
            )

    def test_pypi_version_check_uses_fixed_https_endpoint(self):
        with patch.object(VERIFY_RELEASE, "HTTPSConnection") as connection:
            response = connection.return_value.getresponse.return_value
            response.status = 404

            VERIFY_RELEASE.ensure_version_is_new_on_pypi("2026.8.11+candidate")

            connection.assert_called_once_with("pypi.org", timeout=20)
            connection.return_value.request.assert_called_once_with(
                "GET",
                "/pypi/dal-python/2026.8.11%2Bcandidate/json",
                headers={"Accept": "application/json"},
            )
            connection.return_value.close.assert_called_once_with()

    def test_pypi_version_check_rejects_existing_release(self):
        with patch.object(VERIFY_RELEASE, "HTTPSConnection") as connection:
            response = connection.return_value.getresponse.return_value
            response.status = 200

            with self.assertRaisesRegex(ValueError, "already exists"):
                VERIFY_RELEASE.ensure_version_is_new_on_pypi("2026.8.11")

    def test_pypi_version_check_rejects_unexpected_status(self):
        with patch.object(VERIFY_RELEASE, "HTTPSConnection") as connection:
            response = connection.return_value.getresponse.return_value
            response.status = 503

            with self.assertRaisesRegex(OSError, "HTTP 503"):
                VERIFY_RELEASE.ensure_version_is_new_on_pypi("2026.8.11")


if __name__ == "__main__":
    unittest.main()

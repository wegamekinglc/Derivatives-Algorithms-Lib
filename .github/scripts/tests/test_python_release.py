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
    def write_wheel(
        self,
        directory: Path,
        python_tag: str,
        platform_tag: str,
        *,
        abi_tag: str | None = None,
        metadata_overrides: dict[str, tuple[str, ...]] | None = None,
        wheel_overrides: dict[str, tuple[str, ...]] | None = None,
        extra_metadata_headers: tuple[str, ...] = (),
        extra_wheel_headers: tuple[str, ...] = (),
        native_extensions: int = 1,
        metadata_files: int = 1,
        wheel_files: int = 1,
        metadata_directory: str | None = None,
        wheel_directory: str | None = None,
    ) -> Path:
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        extension = "pyd" if platform_tag == "win_amd64" else "so"
        abi_tag = abi_tag or python_tag
        wheel = directory / (
            f"dal_python-{version}-{python_tag}-{abi_tag}-{platform_tag}.whl"
        )
        dist_info = f"dal_python-{version}.dist-info"
        metadata_headers = {
            "Metadata-Version": ("2.4",),
            "Name": ("dal-python",),
            "Version": (version,),
            "License-Expression": ("MIT",),
            "Requires-Python": (requires_python,),
        }
        metadata_headers.update(metadata_overrides or {})
        wheel_headers = {
            "Wheel-Version": ("1.0",),
            "Root-Is-Purelib": ("false",),
            "Tag": tuple(
                f"{python_tag}-{abi_tag}-{platform}"
                for platform in platform_tag.split(".")
            ),
        }
        wheel_headers.update(wheel_overrides or {})
        metadata_text = "\n".join(
            (
                *(
                    f"{key}: {value}"
                    for key, values in metadata_headers.items()
                    for value in values
                ),
                *extra_metadata_headers,
                "",
                "DAL package description",
            )
        )
        wheel_text = "\n".join(
            (
                *(
                    f"{key}: {value}"
                    for key, values in wheel_headers.items()
                    for value in values
                ),
                *extra_wheel_headers,
                "",
            )
        )
        with ZipFile(wheel, "w") as archive:
            for index in range(native_extensions):
                archive.writestr(f"dal/_dal.{python_tag}.{index}.{extension}", b"native")
            for index in range(metadata_files):
                prefix = metadata_directory or dist_info
                if index:
                    prefix = f"duplicate_{index}.dist-info"
                archive.writestr(f"{prefix}/METADATA", metadata_text)
            for index in range(wheel_files):
                prefix = wheel_directory or dist_info
                if index:
                    prefix = f"duplicate_{index}.dist-info"
                archive.writestr(f"{prefix}/WHEEL", wheel_text)
        return wheel

    def write_matrix(self, directory: Path, python_tags: tuple[str, ...]) -> None:
        for python_tag in python_tags:
            self.write_wheel(directory, python_tag, "manylinux_2_28_x86_64")
            self.write_wheel(directory, python_tag, "win_amd64")

    def test_project_configuration_supports_python_39(self):
        _, requires_python, python_tags = VERIFY_RELEASE.project_configuration()

        self.assertEqual(
            VERIFY_RELEASE.normalized_requires_python(requires_python),
            frozenset((">=3.9", "<3.14")),
        )
        self.assertEqual(
            set(python_tags),
            {"cp39", "cp310", "cp311", "cp312", "cp313"},
        )

    def test_rejects_duplicate_expected_python_selector(self):
        _, _, configured = VERIFY_RELEASE.project_configuration()

        with self.assertRaisesRegex(ValueError, "duplicate selector 'cp39'"):
            VERIFY_RELEASE.validate_expected_python_tags(("cp39", "cp39"), configured)

    def test_accepts_complete_platform_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            version, _, _ = VERIFY_RELEASE.project_configuration()
            self.write_wheel(
                directory,
                "cp313",
                "manylinux_2_27_x86_64.manylinux_2_28_x86_64",
            )
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

    def test_rejects_mixed_linux_platform_tag(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            version, requires_python, _ = VERIFY_RELEASE.project_configuration()
            wheel = self.write_wheel(
                directory,
                "cp39",
                "manylinux_2_28_x86_64.win_amd64",
            )

            with self.assertRaisesRegex(ValueError, "platform component 'win_amd64'"):
                VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_duplicate_requires_python_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            version, requires_python, _ = VERIFY_RELEASE.project_configuration()
            wheel = self.write_wheel(
                directory,
                "cp39",
                "win_amd64",
                extra_metadata_headers=(f"Requires-Python: {requires_python}",),
            )

            with self.assertRaisesRegex(ValueError, "2 Requires-Python headers"):
                VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_compressed_python_and_abi_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            version, requires_python, _ = VERIFY_RELEASE.project_configuration()
            wheel = self.write_wheel(directory, "cp39.cp310", "win_amd64")

            with self.assertRaisesRegex(ValueError, "single CPython tag"):
                VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_expected_python_selector_validation(self):
        _, _, configured = VERIFY_RELEASE.project_configuration()
        cases = (
            ((), "explicit value is empty"),
            (("cp39", ""), "malformed selector"),
            (("cp39", " "), "malformed selector"),
            ((" cp39",), "malformed selector"),
            (("cp3",), "malformed selector"),
            (("cp38",), "not configured"),
        )
        for selectors, message in cases:
            with self.subTest(selectors=selectors), self.assertRaisesRegex(ValueError, message):
                VERIFY_RELEASE.validate_expected_python_tags(selectors, configured)

    def test_rejects_invalid_abi_tags(self):
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        for abi_tag in ("cp310", "abi3", "none"):
            with self.subTest(abi_tag=abi_tag), tempfile.TemporaryDirectory() as tmp:
                wheel = self.write_wheel(
                    Path(tmp), "cp39", "win_amd64", abi_tag=abi_tag
                )
                with self.assertRaisesRegex(ValueError, "Python and ABI|does not match"):
                    VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_invalid_platform_components(self):
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        platforms = (
            "linux_x86_64",
            "manylinux_2_28_x86_64.musllinux_1_2_x86_64",
            "manylinux_2_28_x86_64.macosx_14_0_x86_64",
            "manylinux_2_28_x86_64.win_amd64",
            "manylinux_2_28_x86_64.manylinux_2_28_aarch64",
            "manylinux_2_28_x86_64.manylinux2014_x86_64",
            "manylinux_2_28_x86_64.manylinux_2_29_x86_64",
            "manylinux_2_28_x86_64.manylinux_2_28_x86_64",
            "win32",
            "win_amd64.win32",
        )
        for platform in platforms:
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as tmp:
                wheel = self.write_wheel(Path(tmp), "cp39", platform)
                with self.assertRaises(ValueError):
                    VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_missing_or_duplicate_required_headers(self):
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        for key in ("Name", "Version", "License-Expression", "Requires-Python"):
            for values in ((), ("unexpected", "unexpected")):
                with self.subTest(key=key, values=values), tempfile.TemporaryDirectory() as tmp:
                    wheel = self.write_wheel(
                        Path(tmp),
                        "cp39",
                        "win_amd64",
                        metadata_overrides={key: values},
                    )
                    with self.assertRaisesRegex(ValueError, f"{re.escape(key)} headers"):
                        VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)
        for key in ("Wheel-Version", "Root-Is-Purelib"):
            for values in ((), ("unexpected", "unexpected")):
                with self.subTest(key=key, values=values), tempfile.TemporaryDirectory() as tmp:
                    wheel = self.write_wheel(
                        Path(tmp),
                        "cp39",
                        "win_amd64",
                        wheel_overrides={key: values},
                    )
                    with self.assertRaisesRegex(ValueError, f"{re.escape(key)} headers"):
                        VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_noncanonical_requires_python(self):
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        for value in (
            ">=3.10,<3.14",
            ">=3.9,<3.14,!=3.10",
            ">=3.9,>=3.9,<3.14",
            ">=3.9,<3.14,<3.14",
        ):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as tmp:
                wheel = self.write_wheel(
                    Path(tmp),
                    "cp39",
                    "win_amd64",
                    metadata_overrides={"Requires-Python": (value,)},
                )
                with self.assertRaisesRegex(ValueError, "Requires-Python"):
                    VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_wheel_tag_multiset_drift(self):
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        cases = ((), ("cp39-cp39-win32",), ("cp39-cp39-win_amd64",) * 2)
        for tags in cases:
            with self.subTest(tags=tags), tempfile.TemporaryDirectory() as tmp:
                wheel = self.write_wheel(
                    Path(tmp),
                    "cp39",
                    "win_amd64",
                    wheel_overrides={"Tag": tags},
                )
                with self.assertRaisesRegex(ValueError, "contents and cardinality"):
                    VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_wrong_purelib_value(self):
        with tempfile.TemporaryDirectory() as tmp:
            version, requires_python, _ = VERIFY_RELEASE.project_configuration()
            wheel = self.write_wheel(
                Path(tmp),
                "cp39",
                "win_amd64",
                wheel_overrides={"Root-Is-Purelib": ("true",)},
            )

            with self.assertRaisesRegex(ValueError, "incorrectly marked pure"):
                VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_missing_or_duplicate_archive_members(self):
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        dist_info = rf"dal_python-{re.escape(version)}\.dist-info"
        cases = (
            ({"native_extensions": 0}, "compiled _dal extension"),
            ({"native_extensions": 2}, "compiled _dal extension"),
            ({"metadata_files": 0}, rf"exactly one {dist_info}/METADATA"),
            ({"metadata_files": 2}, rf"exactly one {dist_info}/METADATA"),
            ({"wheel_files": 0}, rf"exactly one {dist_info}/WHEEL"),
            ({"wheel_files": 2}, rf"exactly one {dist_info}/WHEEL"),
        )
        for options, message in cases:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as tmp:
                wheel = self.write_wheel(Path(tmp), "cp39", "win_amd64", **options)
                with self.assertRaisesRegex(ValueError, message):
                    VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_rejects_metadata_outside_filename_dist_info_directory(self):
        version, requires_python, _ = VERIFY_RELEASE.project_configuration()
        cases = (
            ({"metadata_directory": "arbitrary"}, "METADATA"),
            ({"metadata_directory": "wrong.dist-info"}, "METADATA"),
            ({"wheel_directory": "arbitrary"}, "WHEEL"),
            ({"wheel_directory": "wrong.dist-info"}, "WHEEL"),
        )
        for options, member in cases:
            with self.subTest(options=options), tempfile.TemporaryDirectory() as tmp:
                wheel = self.write_wheel(Path(tmp), "cp39", "win_amd64", **options)

                with self.assertRaisesRegex(
                    ValueError,
                    rf"dal_python-{re.escape(version)}\.dist-info/{member}",
                ):
                    VERIFY_RELEASE.validate_wheel(wheel, version, requires_python)

    def test_accepts_reduced_and_complete_matrices(self):
        for python_tags in (
            ("cp39", "cp313"),
            ("cp39", "cp310", "cp311", "cp312", "cp313"),
        ):
            with self.subTest(python_tags=python_tags), tempfile.TemporaryDirectory() as tmp:
                directory = Path(tmp)
                self.write_matrix(directory, python_tags)

                manifest = VERIFY_RELEASE.validate_release(
                    directory, expected_python_tags=python_tags
                )

                self.assertEqual(len(manifest), len(python_tags) * 2)

    def test_rejects_each_missing_newer_target_from_complete_matrix(self):
        configured = ("cp39", "cp310", "cp311", "cp312", "cp313")
        for missing in configured[1:]:
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as tmp:
                directory = Path(tmp)
                self.write_matrix(directory, tuple(tag for tag in configured if tag != missing))
                with self.assertRaisesRegex(ValueError, "wheel matrix mismatch"):
                    VERIFY_RELEASE.validate_release(directory)

    def test_rejects_duplicate_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            self.write_wheel(directory, "cp39", "manylinux_2_28_x86_64")
            self.write_wheel(
                directory,
                "cp39",
                "manylinux_2_27_x86_64.manylinux_2_28_x86_64",
            )
            self.write_wheel(directory, "cp39", "win_amd64")

            with self.assertRaisesRegex(ValueError, "duplicate wheel target"):
                VERIFY_RELEASE.validate_release(
                    directory, expected_python_tags=("cp39",)
                )

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

    def test_python_requirement_order_is_not_significant(self):
        self.assertEqual(
            VERIFY_RELEASE.normalized_requires_python(">=3.10,<3.14"),
            VERIFY_RELEASE.normalized_requires_python("<3.14, >=3.10"),
        )

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

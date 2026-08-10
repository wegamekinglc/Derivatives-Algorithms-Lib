"""Tests for the dal-python binary release verifier."""

import importlib.util
from pathlib import Path
import re
import tempfile
import unittest
from zipfile import ZipFile


SCRIPT = Path(__file__).resolve().parents[3] / "dal-python" / "scripts" / "verify_release.py"
SPEC = importlib.util.spec_from_file_location("verify_release", SCRIPT)
VERIFY_RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY_RELEASE)


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


if __name__ == "__main__":
    unittest.main()

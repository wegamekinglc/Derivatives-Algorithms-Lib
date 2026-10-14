"""Changed numerical inputs or competing runners invalidate evidence."""

from contextlib import ExitStack
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_study_build import digest  # noqa: E402
from lsmc_study_run import dataset_lock, verify_image  # noqa: E402


class IdentityTest(unittest.TestCase):
    def test_each_consumed_source_object_patch_and_image_is_verified(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            files = {name: root / name for name in ("input.cpp", "libdal_cpp.a", "stock", "script_mc_perf", "address-control.patch", "lsmc.cpp")}
            for name, path in files.items():
                path.write_text(name + "\n")
            identity = dict(source=str(root), source_sha256={"input.cpp": digest(files["input.cpp"])},
                            reused_sha256={str(files["libdal_cpp.a"]): digest(files["libdal_cpp.a"])},
                            stock_binary=str(files["stock"]), stock_sha256=digest(files["stock"]),
                            executable=str(files["script_mc_perf"]), executable_sha256=digest(files["script_mc_perf"]),
                            patch_sha256=digest(files["address-control.patch"]), controlled_source_sha256=digest(files["lsmc.cpp"]))
            verify_image(identity)
            for name, path in files.items():
                original = path.read_bytes()
                path.write_bytes(original + b"changed")
                with self.subTest(input=name), self.assertRaises(ValueError):
                    verify_image(identity)
                path.write_bytes(original)
            verify_image(identity)

    @unittest.skipIf(sys.platform == "win32", "the documented experiment runner uses Linux flock")
    def test_competing_runner_is_rejected_until_the_first_releases_its_lock(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with dataset_lock(root):
                with self.assertRaisesRegex(ValueError, "active runner"), ExitStack() as stack:
                    stack.enter_context(dataset_lock(root))
            with dataset_lock(root):
                self.assertTrue((root / "run.lock").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)

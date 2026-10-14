"""A complete storage image cannot silently drift during research."""

from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_storage_run import verify_storage_image  # noqa: E402
from lsmc_study_build import digest  # noqa: E402


class StorageImageTest(unittest.TestCase):
    def test_all_consumed_native_sources_and_build_identities_are_verified(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            files = ("source/input.hpp", "overlay/input.hpp", "build/CMakeCache.txt", "build/compile_commands.json", "image", "patch")
            for name in files:
                path = root / name
                path.parent.mkdir(exist_ok=True)
                path.write_text(name)
            identity = dict(source=str(root / "source"), source_sha256={"input.hpp": digest(root / "source/input.hpp")},
                            overlay=str(root / "overlay"), overlay_sha256={"input.hpp": digest(root / "overlay/input.hpp")},
                            build=str(root / "build"), cache_sha256=digest(root / "build/CMakeCache.txt"),
                            compile_database_sha256=digest(root / "build/compile_commands.json"),
                            executable=str(root / "image"), executable_sha256=digest(root / "image"),
                            patch=str(root / "patch"), patch_sha256=digest(root / "patch"), storage_bits=32)
            verify_storage_image(identity)
            for name in files:
                path = root / name
                path.write_text("changed")
                with self.subTest(name=name), self.assertRaises(ValueError):
                    verify_storage_image(identity)
                path.write_text(name)
            with self.assertRaises(ValueError):
                verify_storage_image(dict(identity, storage_bits=True))


if __name__ == "__main__":
    unittest.main(verbosity=2)

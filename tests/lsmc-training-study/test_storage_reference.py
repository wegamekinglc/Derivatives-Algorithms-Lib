"""Reference reuse requires complete matching native and dependency inputs."""

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_storage_reference import require_reference_sources  # noqa: E402
from lsmc_storage_patch import ROWS, SOURCE_HASHES  # noqa: E402


class StorageReferenceTest(unittest.TestCase):
    def test_changed_or_missing_native_and_dependency_files_reject_reuse(self):
        native = {ROWS: SOURCE_HASHES[ROWS], "dal-cpp/dal/model/example.hpp": "model",
                  "dal-cpp/externals/eigen/Eigen/Core": "eigen", "dal-cpp/benchmarks/example.cpp": "old helper"}
        current = dict(native, **{"dal-cpp/benchmarks/example.cpp": "new helper"})
        require_reference_sources(native, current)
        for name in (ROWS, "dal-cpp/dal/model/example.hpp", "dal-cpp/externals/eigen/Eigen/Core"):
            changed = dict(current, **{name: "drift"})
            with self.subTest(name=name), self.assertRaises(ValueError):
                require_reference_sources(native, changed)
            missing = dict(current)
            del missing[name]
            with self.assertRaises(ValueError):
                require_reference_sources(native, missing)
        added = dict(current, **{"dal-cpp/dal/model/new.hpp": "new native input"})
        with self.assertRaises(ValueError):
            require_reference_sources(native, added)


if __name__ == "__main__":
    unittest.main(verbosity=2)

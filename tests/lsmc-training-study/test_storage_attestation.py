"""Older reference metadata needs actual, matching rebuilt bytes."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_storage_attestation import certified_source_inputs  # noqa: E402
from lsmc_study_build import digest  # noqa: E402


class StorageAttestationTest(unittest.TestCase):
    def test_missing_mismatched_or_changed_rebuild_bytes_reject_certification(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "native.hpp").write_text("native")
            (source / "dependency.hpp").write_text("dependency")
            stock = root / "stock"
            stock.write_text("stock executable bytes")
            image = dict(source=str(source), anchor=32768, executable_sha256="controlled-image",
                         stock_sha256=digest(stock), source_sha256={"native.hpp": digest(source / "native.hpp")}, commands=[])
            entry = dict(anchor=32768, executable_sha256="controlled-image", stock_sha256=digest(stock),
                         fresh_stock=str(stock), identical_controlled_objects=[])
            for index in range(2):
                old, fresh = root / f"old-{index}", root / f"fresh-{index}"
                old.write_text(f"object-{index}")
                fresh.write_bytes(old.read_bytes())
                image["commands"].append(dict(command=["c++", "-o", str(old)]))
                entry["identical_controlled_objects"].append(dict(original=str(old), fresh=str(fresh), sha256=digest(old)))
            inputs = {name: digest(source / name) for name in ("native.hpp", "dependency.hpp")}
            proof = dict(source=str(source), source_sha256=inputs, images=[entry], byte_identical_stock_and_controlled_objects=True)
            build = root / "build"
            build.mkdir()
            for name in ("CMakeCache.txt", "compile_commands.json"):
                (build / name).write_text(name)
            collector = root / "collector.py"
            collector.write_text("rebuild collector")
            proof.update(identical_native_archive=dict(original=str(stock), fresh=str(stock), sha256=digest(stock)),
                         commands=[["cmake", "-B", str(build)]], cache_sha256=digest(build / "CMakeCache.txt"),
                         compile_database_sha256=digest(build / "compile_commands.json"), collector_sha256=digest(collector))
            path = root / "proof.json"
            path.write_text(json.dumps(proof))
            self.assertEqual(certified_source_inputs(image, path), inputs)
            self.assertEqual(certified_source_inputs(image, None), image["source_sha256"])
            for name, value in (("byte_identical_stock_and_controlled_objects", False), ("images", []),
                                ("source_sha256", {"native.hpp": "wrong"})):
                changed = copy.deepcopy(proof)
                changed[name] = value
                path.write_text(json.dumps(changed))
                with self.subTest(name=name), self.assertRaises(ValueError):
                    certified_source_inputs(image, path)
            path.write_text(json.dumps(proof))
            for target in (stock, build / "CMakeCache.txt", build / "compile_commands.json", collector):
                original = target.read_bytes()
                target.write_text("changed rebuild input")
                with self.subTest(target=target.name), self.assertRaises(ValueError):
                    certified_source_inputs(image, path)
                target.write_bytes(original)
            (root / "fresh-1").write_text("changed object")
            with self.assertRaises(ValueError):
                certified_source_inputs(image, path)


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""Admission of complete native storage overlays."""

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
HELPERS = ROOT / "dal-cpp/benchmarks/script_mc_perf"
sys.path.insert(0, str(HELPERS))

SPEC = importlib.util.spec_from_file_location("lsmc_storage_patch", HELPERS / "lsmc_storage_patch.py")
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)

from native_fixture import native_sources


class StorageOverlayTest(unittest.TestCase):
    def test_source_width_and_address_controls_are_explicit(self):
        source = native_sources()
        for bits in (32, 64):
            with self.subTest(bits=bits):
                result = PATCH.storage_sources(source, bits, 32768)
                self.assertEqual(source, native_sources())
                core = result["dal-cpp/dal/script/lsmc.cpp"]
                self.assertIn("StudyReadRow_ x_;", core)
                self.assertIn("StudyReadRow_ pendingH = pending.h_;", core)
                self.assertIn("studyAnchor + validation", core)
                self.assertIn("StudyReadRow_", result["dal-cpp/dal/script/lsmc.hpp"])
                storage = result["dal-cpp/dal/script/visitor/evalstate.hpp"]
                self.assertIn(f"StudyRows_<std::conditional_t<{bits} == 32, float, double>>", storage)
                self.assertIn(f"writer->Uint({bits});", result["dal-cpp/benchmarks/script_mc_perf/lsmctrainingstudy.cpp"])

    def test_unknown_and_repeated_inputs_fail_closed(self):
        source = native_sources()
        for name in source:
            changed = dict(source, **{name: source[name] + "\n// drift\n"})
            with self.subTest(name=name), self.assertRaises(ValueError):
                PATCH.storage_sources(changed, 32, 32768)
        with self.assertRaises(ValueError):
            PATCH.storage_sources(PATCH.storage_sources(source, 32, 32768), 32, 32768)

    def test_invalid_widths_missing_files_and_invalid_anchors_fail(self):
        source = native_sources()
        for bits in (16, 128, True, "32"):
            with self.subTest(bits=bits), self.assertRaises(ValueError):
                PATCH.storage_sources(source, bits, 32768)
        with self.assertRaises(ValueError):
            PATCH.storage_sources({}, 32, 32768)
        with self.assertRaises(ValueError):
            PATCH.storage_sources(source, 32, 0)

    def test_source_admission_uses_utf8_under_a_non_utf8_host_locale(self):
        def host_read(path, encoding=None):
            return path.read_bytes().decode(encoding or "cp1252").replace("\r\n", "\n").replace("\r", "\n")

        original = native_sources()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for newline in ("\n", "\r\n"):
                for name, content in original.items():
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(content.replace("\n", newline).encode("utf-8"))
                with self.subTest(newline=repr(newline)), patch.object(Path, "read_text", host_read):
                    source = PATCH.read_native_sources(root)
                patched = PATCH.storage_sources(source, 32, 32768)
                self.assertIn("StudyReadRow_ x_;", patched[PATCH.CORE])

    def test_current_checkpoint_source_is_outside_the_historical_support_domain(self):
        with self.assertRaisesRegex(ValueError, "unrecognized complete native source: dal-cpp/dal/script/lsmc.cpp"):
            PATCH.storage_sources(PATCH.read_native_sources(ROOT), 32, 32768)


if __name__ == "__main__":
    unittest.main(verbosity=2)

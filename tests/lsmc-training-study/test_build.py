"""Experimental linking preserves stock objects and rejects unsafe commands."""

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
from lsmc_study_build import replacement_compile, replacement_link  # noqa: E402


class BuildCommandTest(unittest.TestCase):
    def setUp(self):
        self.directory = (ROOT / "test-build").resolve()
        self.old = self.directory / "sample.o"
        self.new = self.directory / "control-sample.o"
        self.control = self.directory / "control-lsmc.o"
        self.output = self.directory / "control-image"
        self.command = ["c++", "main.o", "sample.o", "-o", "stock-image", "libdal_cpp.a", "-lpthread", "-Wl,--dependency-file=stock.d"]

    def link(self, command):
        return replacement_link(command, self.directory, self.old, self.new, self.control, self.output)

    def test_only_sample_object_and_output_are_replaced_and_control_precedes_archive(self):
        result, reused = self.link(self.command)
        self.assertEqual(result, ["c++", "main.o", str(self.new), "-o", str(self.output), str(self.control), "libdal_cpp.a", "-lpthread"])
        self.assertEqual(reused, [self.directory / "main.o", self.directory / "libdal_cpp.a"])
        self.assertEqual(self.command[-1], "-Wl,--dependency-file=stock.d")

    def test_unsupported_archive_loading_and_duplicate_symbol_options_are_rejected(self):
        for option in ("-Wl,--whole-archive", "-Wl,--allow-multiple-definition", "-Wl,-z,muldefs", "@link.rsp"):
            with self.subTest(option=option), self.assertRaises(ValueError):
                self.link(self.command + [option])

    def test_missing_or_duplicate_sample_archive_and_output_are_rejected(self):
        commands = (self.command + ["sample.o"], self.command + ["libdal_cpp.a"], self.command + ["-o", "another"],
                    [arg for arg in self.command if arg != "sample.o"], [arg for arg in self.command if arg != "libdal_cpp.a"])
        for command in commands:
            with self.subTest(command=command), self.assertRaises(ValueError):
                self.link(command)

    def test_control_compile_uses_owned_source_output_and_dependency_files(self):
        original = ["c++", "-O3", "-MD", "-MT", "stock.o", "-MF", "stock.d", "-o", "stock.o", "-c", "stock.cpp"]
        command = replacement_compile(original, self.directory / "lsmc.cpp", self.control, 32768)
        self.assertEqual(command[command.index("-c") + 1], str(self.directory / "lsmc.cpp"))
        self.assertEqual(command[command.index("-o") + 1], str(self.control))
        self.assertEqual(command[command.index("-MF") + 1], str(self.control.with_suffix(".d")))
        self.assertEqual(command[command.index("-MT") + 1], str(self.control))
        self.assertIn("-DDAL_LSMC_STUDY_PRICING_OFFSET=32768", command)
        self.assertEqual(original[-1], "stock.cpp")


if __name__ == "__main__":
    unittest.main(verbosity=2)

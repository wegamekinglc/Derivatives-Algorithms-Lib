"""Experimental linking preserves stock objects and rejects unsafe commands."""

from pathlib import Path, PureWindowsPath
import subprocess  # nosec B404: fixed Git setup for an owned temporary input fixture
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "dal-cpp/benchmarks/script_mc_perf"))
import lsmc_study_build as build  # noqa: E402
from lsmc_study_build import program, replacement_compile, replacement_link, require_source_inputs, source_inputs  # noqa: E402


def committed_fixture(root, files):
    root.mkdir(parents=True, exist_ok=True)
    for name in files:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("fixture\n")
    commands = (("init", "-q"), ("add", "."), ("-c", "user.name=Study test", "-c", "user.email=study@example.invalid", "commit", "-qm", "Initial"))
    for arguments in commands:
        subprocess.run([program("git"), *arguments], cwd=root, check=True, shell=False)  # nosemgrep  # nosec B603


class ConsumedInputTest(unittest.TestCase):
    def test_windows_dependencies_use_portable_manifest_keys(self):
        with patch.object(build, "Path", PureWindowsPath), patch.object(build, "tracked_files", side_effect=[["Eigen/Core"], ["include/rapidjson/writer.h"]]):
            names = build.dependency_sources(PureWindowsPath("C:/study"))
        self.assertEqual(names, ["dal-cpp/externals/eigen/Eigen/Core", "dal-cpp/externals/rapidjson/include/rapidjson/writer.h"])

    def test_configuration_and_dependency_headers_are_part_of_consumed_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            configuration = ("CMakeLists.txt", "CMakePresets.json", "dal-cpp/CMakeLists.txt", "dal-cpp/benchmarks/CMakeLists.txt")
            committed_fixture(root, (*configuration, "dal-cpp/dal/lsmc.cpp", "dal-cpp/benchmarks/script_mc_perf/sample.cpp"))
            committed_fixture(root / "dal-cpp/externals/eigen", ("Eigen/Core",))
            committed_fixture(root / "dal-cpp/externals/rapidjson", ("include/rapidjson/writer.h",))
            inputs = source_inputs(root)
            for relative in (*configuration, "dal-cpp/externals/eigen/Eigen/Core", "dal-cpp/externals/rapidjson/include/rapidjson/writer.h"):
                self.assertIn(relative, inputs)
                path = root / relative
                original = path.read_bytes()
                path.write_bytes(original + b"changed")
                with self.subTest(input=relative), self.assertRaises(ValueError):
                    require_source_inputs(root, inputs)
                path.write_bytes(original)


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

"""Exercise the runner's process boundary without benchmark sampling."""

from pathlib import Path
import tempfile
import unittest

from runstudy import Runner


def local_runner(root):
    binary = root / "primal_replay_study"
    binary.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
    binary.chmod(0o700)
    return Runner(binary)


class RunnerTest(unittest.TestCase):
    def test_executable_path_with_shell_characters_is_literal(self):
        with tempfile.TemporaryDirectory(prefix="primal ; $(false) ' ") as root:
            runner = local_runner(Path(root))
            self.assertEqual(["--mode", "verify"], runner.study("verify").splitlines())
            self.assertEqual(["--mode", "replay", "--case", "barrier", "--paths", "17"],
                             runner.study("replay", "barrier", 17).splitlines())

    def test_rejects_program_outside_allowlist(self):
        with tempfile.TemporaryDirectory() as root:
            runner = local_runner(Path(root))
            with self.assertRaises(ValueError):
                runner.capture(["/bin/sh", "-c", "exit 0"])

    def test_rejects_unsupported_study_requests(self):
        with tempfile.TemporaryDirectory() as root:
            runner = local_runner(Path(root))
            for mode, case, paths in (("replay;false", "vanilla", 17), ("replay", "barrier;false", 17),
                                      ("replay", "vanilla", 0), ("replay", "vanilla", 1000001)):
                with self.assertRaises(ValueError):
                    runner.study(mode, case, paths)


if __name__ == "__main__":
    unittest.main()

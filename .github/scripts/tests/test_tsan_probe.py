from pathlib import Path
import subprocess
import tempfile
import unittest


PROBE = Path(__file__).resolve().parents[1] / "tsan" / "probe_runtime.sh"
CLEAN = "messageSize=256\n"
RACE = "WARNING: ThreadSanitizer: data race\n"
FATAL = "FATAL: ThreadSanitizer: runtime failure\n"
WARNING = "WARNING: ThreadSanitizer: thread leak\n"


class TSanProbeTests(unittest.TestCase):
    def test_output_gate(self):
        source = PROBE.read_text(encoding="utf-8")
        function = source[source.index("check_run() {"):source.index('\n"$cxx" --version')]
        harness = (
            "set -euo pipefail\n"
            "runtime=/usr/lib\n"
            "output=$1\n"
            + function
            + '\ncheck_run case "$2" bash -c \'printf "%s" "$1"; exit "$2"\' -- "$3" "$4"\n'
        )
        cases = [
            ("clean", 0, 0, CLEAN, True),
            ("fatal-zero", 0, 0, CLEAN + FATAL, False),
            ("race", 66, 66, RACE, True),
            ("race-and-fatal", 66, 66, RACE + FATAL, False),
            ("fatal-race-exit", 66, 66, FATAL, False),
            ("warning-zero", 0, 0, CLEAN + WARNING, False),
            ("race-zero", 0, 0, CLEAN + RACE, False),
            ("missing-completion", 0, 0, "", False),
            ("missing-race", 66, 66, WARNING, False),
            ("unexpected-race-exit", 0, 66, CLEAN, False),
            ("unexpected-zero-exit", 66, 0, RACE, False),
            ("process-failure", 0, 1, CLEAN, False),
            ("timeout-clean", 0, 124, CLEAN, False),
            ("timeout-race", 66, 124, RACE, False),
        ]
        with tempfile.TemporaryDirectory() as output:
            for name, expected, status, payload, accepted in cases:
                with self.subTest(name=name):
                    result = subprocess.run(
                        ["bash", "-s", "--", output, str(expected), payload, str(status)],
                        input=harness, text=True, capture_output=True, timeout=10, check=False,
                    )
                    self.assertEqual(result.returncode == 0, accepted, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

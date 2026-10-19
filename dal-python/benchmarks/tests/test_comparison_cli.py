"""Comparison entry points must select a build before importing native backends."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
import unittest


BENCHMARKS = Path(__file__).resolve().parents[1]
LAUNCHER = textwrap.dedent("""
    import importlib.abc
    from pathlib import Path
    import runpy
    import sys

    class RejectNativeBackends(importlib.abc.MetaPathFinder):
        def find_spec(self, fullname, path=None, target=None):
            if fullname.split('.')[0] in ('dal', 'QuantLib', 'rateslib'):
                raise RuntimeError('native backend imported before build selection')

    sys.meta_path.insert(0, RejectNativeBackends())
    root = Path(sys.argv.pop(1))
    sys.path.insert(0, str(root))
    sys.argv[0] = str(root / 'run_comparisons.py')
    runpy.run_path(sys.argv[0], run_name='__main__')
""")


def invoke(*arguments):
    env = dict(os.environ)
    env.pop("PYTHONPATH", None)
    env["PYTHONNOUSERSITE"] = "1"
    return subprocess.run(
        [sys.executable, "-s", "-c", LAUNCHER, str(BENCHMARKS), *arguments],
        env=env, capture_output=True, text=True, timeout=30, check=False,
    )


class ComparisonCliTest(unittest.TestCase):
    def test_help_does_not_import_native_backends(self):
        result = invoke("--help")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--dal-package", result.stdout)

    def test_controller_and_workers_reject_missing_selected_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for backend in (None, "dal", "quantlib", "rateslib"):
                with self.subTest(backend=backend):
                    output = root / str(backend)
                    arguments = [
                        "--dal-package", str(root / "missing"),
                        "--output-dir", str(output), "--smoke",
                    ]
                    if backend:
                        arguments += ["--worker", backend]
                    result = invoke(*arguments)

                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("selected DAL build package missing", result.stderr)
                    self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()

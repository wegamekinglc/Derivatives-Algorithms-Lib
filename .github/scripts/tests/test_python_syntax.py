"""Tests for the DAL Python compatibility syntax gate."""

import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "check_dal_python_syntax.py"
SPEC = importlib.util.spec_from_file_location("check_dal_python_syntax", SCRIPT)
CHECK_SYNTAX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK_SYNTAX)


class PythonSyntaxTest(unittest.TestCase):
    def test_inventory_includes_sources_tests_helpers_and_examples(self):
        paths = {path.relative_to(CHECK_SYNTAX.ROOT).as_posix() for path in CHECK_SYNTAX.python_paths()}

        self.assertIn("dal-python/src/dal/__init__.py", paths)
        self.assertIn("dal-python/tests/test_date.py", paths)
        self.assertIn("dal-python/scripts/smoke_installed_wheel.py", paths)
        self.assertIn("dal-python/examples/005.yield_curve_jacobian.py", paths)
        self.assertEqual(CHECK_SYNTAX.syntax_errors(CHECK_SYNTAX.python_paths()), [])


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Compile every Python 3.9-governed DAL Python source without writing bytecode.

Must run under CPython 3.9 exactly: compile() applies the running
interpreter's grammar, so only a 3.9 host enforces the 3.9 grammar floor.
"""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
GRAMMAR_FLOOR = (3, 9)
SOURCE_ROOTS = (
    ROOT / "dal-python" / "src",
    ROOT / "dal-python" / "tests",
    ROOT / "dal-python" / "scripts",
    ROOT / "dal-python" / "examples",
)


def python_paths():
    return tuple(sorted(path for root in SOURCE_ROOTS for path in root.rglob("*.py")))


def syntax_errors(paths):
    errors = []
    for path in paths:
        try:
            source = path.read_text(encoding="utf-8")
            compile(source, str(path), "exec", dont_inherit=True)
        except (OSError, SyntaxError) as error:
            errors.append("%s: %s" % (path.relative_to(ROOT), error))
    return errors


def main():
    if sys.version_info[:2] != GRAMMAR_FLOOR:
        floor = "%d.%d" % GRAMMAR_FLOOR
        print(
            "Python %s syntax gate must run under CPython %s exactly (observed %s); "
            "run: uv run --isolated --no-project --python %s python "
            ".github/scripts/check_dal_python_syntax.py"
            % (floor, floor, sys.version.split()[0], floor),
            file=sys.stderr,
        )
        return 1
    paths = python_paths()
    errors = syntax_errors(paths)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Python syntax checks passed for %d files with %s" % (len(paths), sys.version.split()[0]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

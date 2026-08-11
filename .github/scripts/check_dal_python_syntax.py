#!/usr/bin/env python3
"""Compile every Python 3.9-governed DAL Python source without writing bytecode."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
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
    paths = python_paths()
    errors = syntax_errors(paths)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Python syntax checks passed for %d files with %s" % (len(paths), sys.version.split()[0]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

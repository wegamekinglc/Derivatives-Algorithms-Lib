"""Run build-tree tests without an editable install shadowing the extension."""

import os
from pathlib import Path
import sys


def main():
    build_root, site_packages, *pytest_args = sys.argv[1:]
    os.environ["DAL_PYTHON_BUILD_PACKAGE"] = build_root
    os.environ["PYTHONPATH"] = os.pathsep.join(
        (str(Path(__file__).resolve().parent), build_root, os.environ.get("PYTHONPATH", ""))
    )
    sys.path.insert(0, build_root)
    sys.path.append(site_packages)
    import pytest

    return pytest.main(pytest_args)


if __name__ == "__main__":
    raise SystemExit(main())

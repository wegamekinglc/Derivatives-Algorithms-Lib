"""Keep build-tree subprocesses on the tested DAL extension."""

import os
import sys


build_root = os.environ.get("DAL_PYTHON_BUILD_PACKAGE")
if build_root:
    sys.meta_path[:] = [
        finder for finder in sys.meta_path
        if not (type(finder).__module__.startswith("_editable_") and "dal" in type(finder).__module__)
    ]
    sys.path.insert(0, build_root)

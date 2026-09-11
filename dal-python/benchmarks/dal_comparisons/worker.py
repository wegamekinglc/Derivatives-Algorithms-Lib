"""One backend, one sample per case, in an explicitly selected DAL build."""

from functools import partial
from importlib.machinery import EXTENSION_SUFFIXES
from pathlib import Path
import sys

from dal_benchmarks.harness import Case, Workload, measure, require
from .evidence import SCHEMA, backend_files, package_versions, source_hashes, write_json
from .scenarios import CONVENTIONS, cases
from .suite import prepare


def load_dal(package):
    package = package.resolve()
    require(
        (package / "dal/__init__.py").is_file(), "selected DAL build package missing"
    )
    sys.path.insert(0, str(package))
    import dal

    require(
        any(str(dal._dal.__file__).endswith(suffix) for suffix in EXTENSION_SUFFIXES),
        "DAL native module missing",
    )
    for path in (dal.__file__, dal._dal.__file__):
        require(
            Path(path).resolve().is_relative_to(package / "dal"),
            "DAL import escaped selected build",
        )


def measured_case(backend, case):
    work = prepare(backend, case)
    last = []

    def check(value):
        work.validate(value)
        last[:] = value

    wrapped = Workload(work.run, check)
    definition = Case(case["name"], "third-party", case, lambda: wrapped)
    result = measure(definition, samples=1, warmups=2)
    return dict(result, values=last)


def run(args):
    load_dal(args.dal_package)
    from dal_benchmarks.runner import environment

    provenance = environment()
    require(
        provenance["cmake"].get("CMAKE_BUILD_TYPE:STRING") == "Release",
        "comparison requires a Release DAL build",
    )
    results = list(map(partial(measured_case, args.worker), cases(args.smoke)))
    report = {
        "schema": SCHEMA,
        "status": "passed",
        "backend": args.worker,
        "smoke": args.smoke,
        "conventions": CONVENTIONS,
        "source_hashes": source_hashes(),
        "versions": package_versions(),
        "backend_files": backend_files(args.worker),
        "environment": provenance,
        "results": results,
    }
    write_json(args.output_dir / "worker.json", report)
    return 0

"""Serializable provenance and strict validation of fresh worker reports."""

import hashlib
from importlib import metadata
import json
from pathlib import Path
import statistics

from dal_benchmarks.harness import require
from .scenarios import CONVENTIONS, cases, expected, validate

SCHEMA = "dal.python-comparisons/1"
ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_hashes():
    paths = list((ROOT / "dal_comparisons").glob("*.py"))
    paths += list((ROOT / "dal_benchmarks").glob("*.py"))
    paths += [ROOT / "run_comparisons.py", ROOT / "requirements-comparisons.txt"]
    return {str(path.relative_to(ROOT)): sha256(path) for path in sorted(paths)}


def package_versions():
    return {
        name: metadata.version(name)
        for name in (
            "QuantLib-Python",
            "QuantLib",
            "rateslib",
            "numpy",
            "pandas",
            "matplotlib",
        )
    }


def backend_files(backend):
    module_names = {"dal": "dal", "quantlib": "QuantLib", "rateslib": "rateslib"}
    import sys

    module = sys.modules[module_names[backend]]
    root = Path(module.__file__).resolve().parent
    paths = [Path(module.__file__).resolve()]
    paths += sorted(root.rglob("*.so")) + sorted(root.rglob("*.pyd"))
    return {str(path): sha256(path) for path in paths}


def check_row(row, case):
    require(row["status"] == "passed", "worker case failed")
    require(row["workload"] == case, "comparison workload changed")
    durations = row["samples_ns"]
    require(len(durations) == 1, "worker must return one sample")
    require(type(durations[0]) is int and durations[0] > 0, "invalid timing sample")
    tolerance = 2e-12 if case["operation"] == "discount" else 2e-7
    validate(row["values"], expected(case), abs_tol=tolerance)


def check_report(report, backend, smoke, hashes):
    require(report["schema"] == SCHEMA, "unexpected comparison schema")
    require(report["backend"] == backend, "wrong comparison backend")
    require(report["status"] == "passed", "comparison worker failed")
    require(report["smoke"] == smoke, "comparison profile changed")
    require(
        report["source_hashes"] == hashes, "comparison source changed during sampling"
    )
    require(report["conventions"] == CONVENTIONS, "comparison conventions changed")
    inventory = cases(smoke)
    require(
        [row["name"] for row in report["results"]]
        == [case["name"] for case in inventory],
        "missing, duplicate or reordered comparison cases",
    )
    for row, case in zip(report["results"], inventory):
        check_row(row, case)


def timings(reports, name):
    values = [
        row["samples_ns"][0]
        for report in reports
        for row in report["results"]
        if row["name"] == name
    ]
    return {
        "samples_ns": values,
        "min_ns": min(values),
        "median_ns": statistics.median(values),
    }


def write_json(path, value):
    path.write_text(
        json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8"
    )

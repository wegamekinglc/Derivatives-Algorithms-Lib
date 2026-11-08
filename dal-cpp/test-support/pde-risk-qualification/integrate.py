import argparse
import json
import pathlib
import re
import shutil

import audit
from fixtures import fixtures
from qualify import (
    analyze,
    differences,
    digest,
    is_numerical,
    load,
    normalized,
    write_json,
)
from referenceplan import binding


def link(source, target):
    if not target.exists():
        target.symlink_to(source.resolve())


def reference_origins(old, directory):
    origins = {}
    for source in old.glob("N12ref-*"):
        link(source, directory / source.name)
        origins[source.name] = dict(source=str(source), sha256=digest(source))
    write_json(directory / "parallel-reference-origins.json", origins)


def prepare_view(old, directory, view):
    view.mkdir(parents=True, exist_ok=True)
    origins = {}
    for name in fixtures():
        current = (directory / (name + ".json.gz")).exists()
        source = directory if current else old
        origins[name] = "current" if current else "historical"
        for suffix in [".json.gz", ".input.json"]:
            link(source / (name + suffix), view / (name + suffix))
    for name in ["supplement.json", "n9a-audit.json"]:
        shutil.copyfile(directory / name, view / name)
    write_json(view / "origins.json", origins)
    return origins


def dependencies(name, row, names):
    evidence = row.get("evidence")
    if isinstance(evidence, dict) and "fixtures" in evidence:
        return evidence["fixtures"]
    special = {
        "N8-0-implicit-comparison": ["N8-0", "N1-0-100-0.03"],
        "N8-1-implicit-comparison": ["N8-1", "N3-0-100"],
        "N10b-zero": ["N10b-0"],
        "N10b-small": ["N10b-1e-08"],
        "N9b-control": ["N9b-100000000.0-100"],
        "N9a-layers": ["N9a", "N9a-observed"],
        "supplement": ["supplement.json"],
    }
    if name in special:
        return special[name]
    candidates = [n for n in names if name == n or name.startswith(n + "-")]
    if not candidates:
        raise RuntimeError("unmapped audit dependency: " + name)
    return [max(candidates, key=len)]


def admit_audits(view, directory, origins):
    audit.summarize(view)
    records = json.loads((view / "audits.json").read_text())
    admitted = {}
    for name, row in records.items():
        required = dependencies(name, row, fixtures())
        historical = [n for n in required if origins.get(n) == "historical"]
        admitted[name] = dict(row, required_cases=required, historical_cases=historical)
        if historical:
            admitted[name].update(status="INCOMPLETE", historical_status=row["status"])
    write_json(directory / "audits-admitted.json", admitted)
    return admitted


def numerical_rows(directory, references):
    shutil.copyfile(references, directory / "references.json")
    analyze(directory)
    rows = json.loads((directory / "summary.json").read_text())
    refs = json.loads(references.read_text())
    for name, row in rows.items():
        key = row.get("reference")
        if key in refs and (directory / (name + ".json.gz")).exists():
            row["raw_reference_difference"] = differences(
                normalized(load(directory, name)), refs[key].get("normalized", [])
            )
            row["comparison_admitted"] = refs[key]["status"] == "PASS"
    write_json(directory / "numerical-results.json", rows)
    return rows


def verdict(statuses):
    if "FAIL" in statuses:
        return "FAIL"
    return "INCOMPLETE" if "INCOMPLETE" in statuses else "PASS"


def group_id(name):
    match = re.match(r"N\d+", name)
    return match.group() if match else None


def validate_direct_evidence():
    path = pathlib.Path(".codex/artifacts/validation/dal502-repair/identity.json")
    identity = json.loads(path.read_text())
    for name in [
        "dal-cpp/dal/math/pde/earlyexercisepde.cpp",
        "dal-cpp/tests/math/pde/test_early_exercise_pde.cpp",
    ]:
        if digest(pathlib.Path(name)) != identity["source_sha256"][name]:
            raise RuntimeError("accepted direct-test source changed: " + name)
    library = pathlib.Path("build/dal502-make/dal-cpp/libdal_cpp.a")
    if digest(library) != identity["files_sha256"][str(library)]:
        raise RuntimeError("accepted direct-test library changed")
    tests = identity["tests"]["final-direct.xml"]
    if tests["tests"] != "51" or tests["failures"] != "0":
        raise RuntimeError("accepted direct-test result missing")


def group_numerics(group, rows):
    return {
        name: row["status"]
        for name, row in rows.items()
        if group_id(name) == group
        and (
            is_numerical(name) or name == "N1-default" or name.startswith("N11-reject")
        )
    }


def current_audit_group(group, audits):
    rows = {
        name: row["status"] for name, row in audits.items() if group_id(name) == group
    }
    if group == "N9":
        validate_direct_evidence()
        rows = {
            name: status
            for name, status in rows.items()
            if not name.startswith(("N9b", "N9d"))
        }
        rows["accepted-repair-51-test-controls"] = "PASS"
    return rows


def group_results(directory, rows, audits):
    result = {}
    for i in range(1, 13):
        group = f"N{i}"
        numeric = group_numerics(group, rows)
        checks = current_audit_group(group, audits)
        if group in ["N2", "N10", "N12"]:
            checks["supplement"] = audits["supplement"]["status"]
        statuses = list(numeric.values()) + list(checks.values())
        result[group] = dict(status=verdict(statuses), numerical=numeric, audits=checks)
    write_json(directory / "groups.json", result)
    return result


def reference_bindings(directory, references):
    refs = json.loads(references.read_text())
    result = {}
    for name, request in fixtures().items():
        if is_numerical(name):
            key = binding(request)
            result[name] = dict(
                binding=key,
                actual_scales=refs.get(key, {}).get("actual_scales"),
                reference_status=refs.get(key, {}).get("status", "INCOMPLETE"),
            )
    write_json(directory / "fixture-reference-bindings.json", result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("phase", choices=["prepare", "analyze"])
    parser.add_argument("--directory", type=pathlib.Path, required=True)
    parser.add_argument(
        "--old", type=pathlib.Path, default=pathlib.Path("build/dal502-evidence")
    )
    parser.add_argument("--references", type=pathlib.Path)
    args = parser.parse_args()
    if args.phase == "prepare":
        reference_origins(args.old, args.directory)
        return
    view = args.directory.parent / "audit-view"
    origins = prepare_view(args.old, args.directory, view)
    audits = admit_audits(view, args.directory, origins)
    rows = numerical_rows(args.directory, args.references)
    reference_bindings(args.directory, args.references)
    groups = group_results(args.directory, rows, audits)
    print({name: row["status"] for name, row in groups.items()})


if __name__ == "__main__":
    main()

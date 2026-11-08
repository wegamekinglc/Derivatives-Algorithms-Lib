import argparse
import gzip
import json
import math
import pathlib

from batchplan import collect, initialize, schedule
from fixtures import fixtures
from qualify import (
    derivatives,
    digest,
    differences,
    is_numerical,
    normalize_reference,
    scalar_price,
    within,
    write_json,
)
from referenceplan import (
    binding,
    estimates,
    fingerprint,
    mean_members,
    offsets,
    physical,
    price_key,
    price_spec,
    self_qualification,
    scales,
)

OLD_LEVELS = [4096, 8192, 16384]
NEW_LEVELS = [32768, 65536, 131072]


def engine(name):
    if name.startswith("N3-") or name == "N8-1":
        return "tree"
    if name.startswith(("N6-", "N7-")):
        return ["analytic", "tree", "tree", "log"][int(name.split("-")[1])]
    return "log" if name.startswith(("N4-", "N12-")) else "analytic"


def controls():
    return {name: r for name, r in fixtures().items() if is_numerical(name) and name != "N1-default"}


def unique_controls(mode):
    return {binding(r): r for name, r in controls().items() if engine(name) == mode}


def specs_for(request, mode, levels):
    for level in levels:
        for parity in range(2 if mode == "tree" else 1):
            for axis, offset in offsets(request):
                yield price_spec(request, axis, offset, mode, level + parity)


def analytic_spec(request):
    return price_spec(request, "base", 0.0, "analytic", 0)


def lookup_key(spec):
    return fingerprint(dict(mode=spec["mode"], request=spec["request"]))


def raw_result(path):
    with gzip.open(path, "rt") as stream:
        result = json.load(stream)
    result.pop("elapsed_seconds", None)
    result.pop("returncode", None)
    return result


def legacy_index(old):
    manifest = json.loads((old / "raw-file-manifest.json").read_text())
    result = {}
    for path in old.glob("*-ref-L*-P*-*.input.json"):
        request = json.loads(path.read_text())
        mode = "tree" if path.name.startswith("N3-") else "log"
        selector = {"tree_steps": request["tree_steps"]} if mode == "tree" else {"level": request["level"]}
        normalized = dict(physical(request), **selector)
        output = old / path.name.replace(".input.json", ".json.gz")
        if digest(output) != manifest[output.name]["sha256"]:
            raise RuntimeError("legacy reference checksum mismatch: " + output.name)
        key = fingerprint(dict(mode=mode, request=normalized))
        result[key] = dict(result=raw_result(output), path=str(output.resolve()), sha256=digest(output))
    return result


def equivalence_specs(old):
    requests = fixtures()
    specs = list(specs_for(requests["N3-0-100"], "tree", OLD_LEVELS))
    for name in ["N3-0-120", "N3-1-100", "N3-1-120"]:
        specs += list(specs_for(requests[name], "tree", [4096]))
    specs += list(specs_for(requests["N4-0-100-137"], "log", [0, 1, 2]))
    refs = json.loads((old / "references.json").read_text())
    specs += [analytic_spec(requests[name]) for name, row in refs.items() if "values" in row]
    return specs


def prepare_equivalence(directory, old, executable):
    ledger = initialize(directory, executable)
    index = legacy_index(old)
    specs = equivalence_specs(old)
    expected = {}
    for spec in specs:
        key = price_key(spec, ledger["identity"])
        expected[key] = expected_result(spec, index, old)
    ledger["equivalence_expected"] = expected
    schedule(directory, ledger, specs, "equivalence")


def expected_result(spec, index, old):
    if spec["mode"] != "analytic":
        return index[lookup_key(spec)]
    for name, request in fixtures().items():
        path = old / (name + "-ref.json.gz")
        if path.exists() and physical(request) == physical(spec["request"]):
            return dict(result=raw_result(path), path=str(path.resolve()), sha256=digest(path))
    raise RuntimeError("missing analytic equivalence witness")


def check_equivalence(ledger):
    comparisons = []
    for key, expected in ledger["equivalence_expected"].items():
        actual = ledger["cache"].get(key)
        comparisons.append(
            dict(id=key, path=expected["path"], sha256=expected["sha256"], equal=actual == expected["result"])
        )
    if not all(row["equal"] for row in comparisons):
        raise RuntimeError("reference price equivalence failed or has missing rows")
    return comparisons


def all_old_specs():
    for mode in ["tree", "log"]:
        levels = OLD_LEVELS if mode == "tree" else [0, 1, 2]
        for request in unique_controls(mode).values():
            yield from specs_for(request, mode, levels)


def import_legacy(directory, ledger, old, index):
    identity_path = pathlib.Path(".codex/artifacts/validation/dal502/identity.json")
    identity = json.loads(identity_path.read_text())
    origin = dict(
        manifest=digest(old / "raw-file-manifest.json"),
        identity=digest(identity_path),
        executable=identity["binaries"]["build/dal502-driver"],
    )
    ledger["legacy_origin"] = origin
    for spec in all_old_specs():
        row = index.get(lookup_key(spec))
        if row is not None:
            old_key = price_key(spec, origin)
            ledger["cache"][old_key] = row["result"]
            ledger["bridges"][price_key(spec, ledger["identity"])] = old_key
    write_json(directory / "ledger.json", ledger)


def read_price(ledger, spec):
    key = price_key(spec, ledger["identity"])
    actual = ledger["cache"].get(key)
    if actual is None:
        actual = ledger["cache"].get(ledger["bridges"].get(key))
    if actual is None or "error" in actual:
        raise RuntimeError("reference node missing/failed: " + key)
    value = actual["price"]
    if value is None or not math.isfinite(value):
        raise RuntimeError("reference node nonfinite: " + key)
    return value


def level_estimates(ledger, request, mode, level):
    members = []
    for parity in range(2 if mode == "tree" else 1):

        def price(axis, offset):
            return read_price(ledger, price_spec(request, axis, offset, mode, level + parity))

        members.append(estimates(request, price))
    return dict(members=members, mean=mean_members(members))


def tree_or_log(ledger, request, mode, levels):
    try:
        values = [level_estimates(ledger, request, mode, n) for n in levels]
        result = self_qualification(values, mode)
        result.update(actual_scales=scales(request), levels_used=levels)
        return result
    except RuntimeError as error:
        return dict(status="INCOMPLETE", reason=str(error), levels_used=levels)


def verify_self_equivalence(directory, ledger, old):
    expected = json.loads((old / "references.json").read_text())
    result = {}
    for name in [n for n in expected if "levels" in expected[n]]:
        mode = engine(name)
        levels = OLD_LEVELS if mode == "tree" else [0, 1, 2]
        actual = tree_or_log(ledger, fixtures()[name], mode, levels)
        fields = ["levels", "mesh_difference", "bump_difference", "parity_differences", "normalized", "status"]
        result[name] = all(actual.get(k) == expected[name].get(k) for k in fields)
    write_json(directory / "self-equivalence.json", result)
    if not all(result.values()):
        raise RuntimeError("reference self-qualification equivalence failed")


def accept_equivalence(directory, old):
    ledger = collect(directory)
    comparisons = check_equivalence(ledger)
    write_json(directory / "price-equivalence.json", comparisons)
    import_legacy(directory, ledger, old, legacy_index(old))
    verify_self_equivalence(directory, ledger, old)
    print("equivalent prices", len(comparisons), "legacy metric controls verified")


def prepare_initial(directory):
    ledger = collect(directory)
    specs = list(all_old_specs())
    specs += [analytic_spec(r) for r in unique_controls("analytic").values()]
    schedule(directory, ledger, specs, "initial-scales")


def successful_reference_job(row):
    result = row.get("result", {})
    price = result.get("price")
    return row["state"] == "COMPLETE" and "error" not in result and price is not None and math.isfinite(price)


def completed_level(ledger, n):
    jobs = [
        row
        for row in ledger["jobs"].values()
        if row["spec"]["mode"] == "tree" and row["spec"]["request"]["tree_steps"] in [n, n + 1]
    ]
    return bool(jobs) and all(successful_reference_job(row) for row in jobs)


def extension_guard(directory, ledger, n):
    previous = {65536: 32768, 131072: 65536}.get(n)
    if previous is not None and not completed_level(ledger, previous):
        raise RuntimeError("previous approved paired level is incomplete")
    if n == 131072:
        latest = json.loads((directory / "references-65536.json").read_text())
        tree = [row for row in latest.values() if row["engine"] == "tree"]
        if all(row["status"] == "PASS" for row in tree):
            raise RuntimeError("latest level qualified; further extension is not approved")


def prepare_level(directory, n):
    if n not in NEW_LEVELS:
        raise RuntimeError("unapproved reference extension")
    ledger = collect(directory)
    extension_guard(directory, ledger, n)
    specs = []
    for request in unique_controls("tree").values():
        specs += list(specs_for(request, "tree", [n]))
    schedule(directory, ledger, specs, "level-" + str(n))


def analytic(ledger, request):
    spec = analytic_spec(request)
    actual = ledger["cache"].get(price_key(spec, ledger["identity"]))
    if actual is None or "values" not in actual:
        return dict(status="INCOMPLETE", reason="analytic formula node missing")
    values = actual["values"]
    independent = derivatives(scalar_price, request, 0.001, 1e-5, 1e-5)
    diff = differences(normalize_reference(values), normalize_reference(independent))
    return dict(
        status="PASS" if within(diff, 0.25, True) else "INCOMPLETE",
        normalized=normalize_reference(values),
        formula_difference=diff,
    )


def qualify(directory, top):
    ledger = collect(directory)
    result = {}
    for name, request in controls().items():
        mode = engine(name)
        if mode == "analytic":
            row = analytic(ledger, request)
        else:
            levels = [0, 1, 2] if mode == "log" else [n for n in OLD_LEVELS + NEW_LEVELS if n <= top]
            row = tree_or_log(ledger, request, mode, levels)
        row.update(engine=mode, binding=binding(request), execution_identity=ledger["identity"])
        if "budget_stop" in ledger:
            row.update(status="INCOMPLETE", reason=ledger["budget_stop"])
        result[binding(request)] = row
    write_json(directory / ("references-" + str(top) + ".json"), result)
    print("reference bindings", len(result), "incomplete", sum(v["status"] != "PASS" for v in result.values()))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("phase", choices=["equivalence", "accept", "initial", "level", "qualify"])
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--old", type=pathlib.Path, default=pathlib.Path("build/dal502-evidence"))
    parser.add_argument("--driver", type=pathlib.Path, default=pathlib.Path("build/dal502-tools/dal502_driver"))
    parser.add_argument("--level", type=int, default=16384)
    args = parser.parse_args()
    actions = dict(
        equivalence=lambda: prepare_equivalence(args.output, args.old, args.driver),
        accept=lambda: accept_equivalence(args.output, args.old),
        initial=lambda: prepare_initial(args.output),
        level=lambda: prepare_level(args.output, args.level),
        qualify=lambda: qualify(args.output, args.level),
    )
    actions[args.phase]()


if __name__ == "__main__":
    main()

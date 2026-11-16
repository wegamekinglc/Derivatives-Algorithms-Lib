"""Frozen layer reuse and append-only corrected-domain evidence for A/2."""

import json
from dataclasses import asdict

import numpy as np
from a1_axes import MATURITIES
from a1_budget import digest
from a1_pde import mesh
from a1_run import execute, read, seal, write
from a1_study import black_model, canonical_logs, case_strikes, make_model
from pde import timeline

SCHEME = "A1_NONUNIFORM_CN_MIDPOINT_2_HALF_IE/1"


def require_mapping(actual, expected):
    if actual != expected:
        raise RuntimeError("Frozen layer mapping mismatch")


def bits_equal(a, b):
    return np.array_equal(np.asarray(a).view(np.uint64), np.asarray(b).view(np.uint64))


def reusable_layer(row, x, times, scheme):
    return (
        not row["expanded"]
        and bits_equal(row["mesh"], x)
        and bits_equal(row["timeline"], times)
        and row["scheme"] == scheme
        and row["actual_steps"] == len(times) - 1
    )


def config(row):
    return row["level"], row["requested_steps"], row["expanded"]


def signature(argument):
    model, time, strikes, knots, setting = argument
    value = dict(
        model=asdict(model),
        time=time,
        strikes=strikes,
        knots=list(knots),
        setting=setting,
    )
    return seal(json.loads(json.dumps(value, default=lambda a: a.tolist())))


def black_cases(request):
    grid = request["grids"][7]
    for flat in (False, True):
        model = black_model(grid, flat)
        for i, time in enumerate(MATURITIES):
            yield (
                f"Black_{'flat' if flat else 'knots'}_T{i}",
                model,
                time,
                canonical_logs(grid["spots"]),
                True,
            )


def old_cases(old):
    for fixture in old["fixtures"]:
        if not fixture["id"].startswith("F2"):
            continue
        grid = next(g for g in fixture["grids"] if g["id"] == "tau_quarter")
        model = make_model(fixture, grid)
        for i, time in enumerate(MATURITIES):
            yield (
                fixture["id"] + "_old_T" + str(i),
                model,
                time,
                canonical_logs(grid["spots"]),
                False,
            )


def arguments(case, settings):
    _, model, time, knots, _ = case
    strikes = case_strikes(time, model.flat)
    return [(model, time, strikes, knots, setting) for setting in settings]


def checked_central(row, argument):
    model, time, strikes, knots, setting = argument
    x = mesh(knots, canonical_logs(strikes), setting[0])
    times = timeline(time, setting[1], model.times)
    if not reusable_layer(row, x, times, SCHEME):
        raise RuntimeError("Central layer input/mesh/timeline/scheme mismatch")


def case_mapping(root, case):
    name = case[0]
    path = root / (name + ".json")
    if not path.exists():
        return []
    result = []
    for index, row in enumerate(read(path)["levels"]):
        argument = arguments(case, [config(row)])[0]
        if not row["expanded"]:
            checked_central(row, argument)
        result.append(
            dict(
                study=name,
                index=index,
                source_sha256=digest(path),
                layer_sha256=seal(row),
                setting=list(config(row)),
                input_sha256=signature(argument),
                action="P3_RECOMPUTE" if row["expanded"] else "CENTRAL_REUSE",
            )
        )
    return result


def reuse_plan(root):
    cases = list(black_cases(read(root / "request.json")))
    cases += list(old_cases(read(root / "old-capture.json")))
    result = [r for case in cases for r in case_mapping(root, case)]
    actions = [r["action"] for r in result]
    if actions.count("CENTRAL_REUSE") != 48 or actions.count("P3_RECOMPUTE") != 10:
        raise RuntimeError("Expected 48 central / 10 expanded mapping")
    return result


def layer_path(name, setting):
    level, steps, expanded = setting
    return "layers/" + name + f"-{level}-{steps}-{int(expanded)}.json"


def checkpoint(directory, budget, files, event):
    registry = budget.state.setdefault(registry_key(directory), {})
    for name in files:
        registry[name] = digest(directory / name)
    budget.save("checkpoint:" + event)


def validate_checkpoint(directory, budget):
    for name, expected in budget.state.get(registry_key(directory), {}).items():
        path = directory / name
        if not path.is_file() or digest(path) != expected:
            raise RuntimeError("Checkpoint evidence hash mismatch: " + name)


def registry_key(directory):
    return "a3_files" if directory.name == "a3" else "a2_files"


class LayerRunner:
    def __init__(self, root, directory, budget, mapping):
        self.root, self.directory, self.budget = root, directory, budget
        self.mapping = {(r["study"], tuple(r["setting"])): r for r in mapping}
        (directory / "layers").mkdir(exist_ok=True)

    def source(self, name, argument):
        record = self.mapping.get((name, tuple(argument[-1])))
        if not record:
            return None
        if signature(argument) != record["input_sha256"]:
            raise RuntimeError("Mapped model/input identity mismatch")
        path = self.root / (name + ".json")
        if digest(path) != record["source_sha256"]:
            raise RuntimeError("Mapped source hash mismatch")
        return record

    def cached(self, name, argument):
        relative = layer_path(name, argument[-1])
        path = self.directory / relative
        if not path.exists():
            return None
        expected = self.budget.state.get("a2_files", {}).get(relative)
        record = read(path)
        if digest(path) != expected or record["input_sha256"] != signature(argument):
            raise RuntimeError("Layer checkpoint identity/hash mismatch")
        return record["value"]

    def save(self, name, argument, value, origin, kind):
        relative = layer_path(name, argument[-1])
        write(
            self.directory / relative,
            dict(
                value=value,
                input_sha256=signature(argument),
                provenance=kind,
                source=origin,
                execution_identity=self.budget.state["identity"],
            ),
        )
        checkpoint(self.directory, self.budget, [relative], name)
        return value

    def prepare(self, name, argument):
        cached = self.cached(name, argument)
        if cached is not None:
            return cached
        origin = self.source(name, argument)
        if origin and origin["action"] == "CENTRAL_REUSE":
            row = read(self.root / (name + ".json"))["levels"][origin["index"]]
            checked_central(row, argument)
            return self.save(name, argument, row, origin, "REUSED_ORIGINAL_CENTRAL")
        return None

    def __call__(self, args, name):
        values = [self.prepare(name, argument) for argument in args]
        pending = [(i, a) for i, a in enumerate(args) if values[i] is None]
        if pending:
            fresh = execute([a for _, a in pending], name, self.directory, self.budget)
            for (index, argument), value in zip(pending, fresh):
                origin = self.source(name, argument)
                values[index] = self.save(
                    name, argument, value, origin, "NEW_EXECUTION"
                )
        return values

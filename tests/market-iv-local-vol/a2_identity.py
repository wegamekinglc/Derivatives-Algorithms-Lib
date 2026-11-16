"""Explicit A/1-to-A/2 migration of the same cumulative budget."""

import ast
import json

from a1_budget import Budget, digest

CONTROL_SHA = "84a6de0aa9c0773e74022a5d21ab865291c15fed37fe913f5b53f496d75e485a"
APPROVAL = "01a1e52a-e892-7771-920e-3a5dc20e6384"
ORIGIN = dict(
    manifest="f73ff4565e011a8fee246840be67637beb8f8a00077997779440889f00b277cf",
    ledger="3bf459b95be10ba8a13a3d4b22c4f424bfc164cfa5aceb0ae31db103c1fb95d2",
    journal="651f549f358dd88afc31cbc7b9641348b4864a90c00f593025c62d35f4ccbecb",
    sequence=180,
    identity="a23afd16afef0b73374f648264f03fd269dc90820207141b0da9cfe6e27181ba",
)
NAMES = dict(manifest="manifest.json", ledger="ledger.json", journal="ledger.jsonl")


def verify_origin(root, expected):
    for key, name in NAMES.items():
        path = root / name
        if not path.is_file() or digest(path) != expected[key]:
            raise RuntimeError("Original sealed evidence mismatch: " + name)


def archive_origin(root, expected):
    saved = root / "a2/origin"
    saved.mkdir(parents=True, exist_ok=True)
    for name in NAMES.values():
        target = saved / name
        if target.exists() and target.read_bytes() != (root / name).read_bytes():
            raise RuntimeError("Sealed origin cannot be overwritten")
        if not target.exists():
            target.write_bytes((root / name).read_bytes())
    verify_origin(saved, expected)


def migrate(root, identity, expected, metadata):
    verify_origin(root, expected)
    budget = Budget(root / "ledger.json", expected["identity"])
    if budget.state["sequence"] != expected["sequence"] or budget.state["active_phase"]:
        raise RuntimeError("Original transition boundary mismatch")
    archive_origin(root, expected)
    transition = dict(metadata, **{k: expected[k] for k in NAMES})
    transition.update(
        old_identity=expected["identity"],
        new_identity=identity,
        origin_sequence=expected["sequence"],
    )
    budget.state.update(
        identity=identity,
        origin_identity=expected["identity"],
        origin_directory=str(root.resolve()),
        transition=transition,
    )
    budget.save("transition:A/2")
    return budget


def verify_migrated(root, identity):
    verify_origin(root / "a2/origin", ORIGIN)
    if digest(root / "manifest.json") != ORIGIN["manifest"]:
        raise RuntimeError("Original manifest cannot be rebound")
    budget = Budget(root / "ledger.json", identity)
    transition = budget.state.get("transition", {})
    expected = dict(
        old_identity=ORIGIN["identity"],
        new_identity=identity,
        approval=APPROVAL,
        control_sha256=CONTROL_SHA,
    )
    if any(transition.get(k) != v for k, v in expected.items()):
        raise RuntimeError("Execution transition identity mismatch")
    return budget


def kernels(path):
    names = {
        "derivative_weights",
        "operator",
        "step",
        "solve",
        "configurations",
        "admitted",
    }
    return {
        n.name: ast.dump(n, include_attributes=False)
        for n in ast.parse(path.read_text()).body
        if isinstance(n, ast.FunctionDef) and n.name in names
    }


def verify_reuse_dependencies(root, source, core):
    old = json.loads((root / "manifest.json").read_text())["core"]
    for key in ("dependency_pins", "compiler", "python", "packages"):
        if old[key] != core[key]:
            raise RuntimeError("Reference reuse dependency mismatch: " + key)
    for name in (
        "pde.py",
        "reference.py",
        "a1_study.py",
        "grid_study.py",
        "qualify.py",
    ):
        if digest(source / name) != digest(root / "executed-source" / name):
            raise RuntimeError("Reference reuse source mismatch: " + name)
    if kernels(source / "a1_pde.py") != kernels(root / "executed-source/a1_pde.py"):
        raise RuntimeError("Reference reuse numerical kernel mismatch")

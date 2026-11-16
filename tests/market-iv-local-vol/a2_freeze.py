"""Freeze an A/2 execution extension, never a new source budget."""

from a1_budget import digest
from a1_process import output, program
from a1_run import (
    CONTROL,
    HERE,
    REPO,
    execution_core,
    execution_inventory,
    read,
    save_namespace,
    seal,
    write,
)
from a2_identity import (
    APPROVAL,
    CONTROL_SHA,
    ORIGIN,
    migrate,
    verify_migrated,
    verify_origin,
    verify_reuse_dependencies,
)
from a2_reference import require_mapping, reuse_plan, validate_checkpoint


def fixed_root(root):
    if root.resolve() != (REPO / "build/dal-542-a1").resolve():
        raise RuntimeError("A/2 must extend the original numerical workspace")


def verify_initial_state(root):
    verify_origin(root, ORIGIN)
    state = read(root / "ledger.json")
    expected = dict(
        solves_reserved=58,
        solves_completed=58,
        paths=[0, 0],
        paths_completed=[0, 0],
        active_phase=None,
        total_seconds=20.26435111698811,
    )
    if any(state[k] != v for k, v in expected.items()):
        raise RuntimeError("Sealed A/1 accounting boundary mismatch")
    if state["seconds"] != {"pde": 20.26435111698811}:
        raise RuntimeError("Sealed A/1 phase time mismatch")
    original = read(root / "manifest.json")
    if seal(original["core"]) != ORIGIN["identity"]:
        raise RuntimeError("Original execution seal mismatch")
    for name, expected_hash in original["namespace_files"].items():
        if digest(root / name) != expected_hash:
            raise RuntimeError("Original namespace input mismatch")


def extension_core(root, original, binaries):
    sources, paths = execution_inventory(binaries, original)
    core = execution_core(paths, original, binaries)
    verify_reuse_dependencies(root, HERE, core)
    core.update(
        schema="dal.market-iv-a2-execution/1",
        origin=ORIGIN,
        approval=APPROVAL,
        reuse_plan=reuse_plan(root),
        maximum_foreground_seconds=21600,
        default_foreground_seconds=1800,
        gate_order=[
            "P3_expanded_10",
            "Black_8",
            "old_diagnostics_8",
            "capture_N4_probes",
            "P7_P8_16_grid",
            "diagnostics_56",
            "default_alignment",
            "MC",
        ],
        capacity_paths=dict(conditional=354, uncapped_worst=386, hard_cap=376),
    )
    core["origin_inputs"] = {
        n: digest(root / n)
        for n in ("request.json", "old-capture.json", "history.json")
    }
    return sources, core


def freeze_a2(root, original, binaries):
    fixed_root(root)
    verify_initial_state(root)
    directory = root / "a2"
    if directory.exists():
        raise RuntimeError("A/2 extension already exists; cannot recreate or reset")
    if digest(CONTROL) != CONTROL_SHA:
        raise RuntimeError("A/2 frozen control mismatch")
    if output("git", ["-C", str(REPO), "status", "--porcelain"]):
        raise RuntimeError("Commit tested A/2 source before execution freeze")
    if binaries.resolve() != program("capture").parent.resolve():
        raise RuntimeError("A/2 binary directory mismatch")
    sources, core = extension_core(root, original, binaries)
    identity = seal(core)
    directory.mkdir()
    save_namespace(directory, original, sources)
    write(directory / "manifest.json", dict(core=core, identity=identity))
    budget = migrate(
        root,
        identity,
        ORIGIN,
        dict(
            approval=APPROVAL,
            control_sha256=CONTROL_SHA,
            manifest_sha256=digest(directory / "manifest.json"),
        ),
    )
    if budget.state["sequence"] != 181:
        raise RuntimeError("Transition must be event 181")
    print("A/2 frozen", identity, "transition=181", flush=True)


def validate_files(root, core):
    for name, expected in core["files"].items():
        if digest(name) != expected:
            raise RuntimeError("Actual execution identity mismatch: " + name)
    for name, expected in core["origin_inputs"].items():
        if digest(root / name) != expected or digest(root / "a2" / name) != expected:
            raise RuntimeError("A/2 input identity mismatch: " + name)


def validate_a2(root):
    fixed_root(root)
    manifest = read(root / "a2/manifest.json")
    core = manifest["core"]
    if seal(core) != manifest["identity"] or core["control_sha256"] != CONTROL_SHA:
        raise RuntimeError("A/2 execution seal mismatch")
    validate_files(root, core)
    require_mapping(reuse_plan(root), core["reuse_plan"])
    budget = verify_migrated(root, manifest["identity"])
    if budget.state["transition"]["manifest_sha256"] != digest(
        root / "a2/manifest.json"
    ):
        raise RuntimeError("A/2 manifest cannot be rebound")
    validate_checkpoint(root / "a2", budget)
    return manifest, budget

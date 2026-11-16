"""One-time controller repair: validate existing native tiny without redrawing it."""

from a1_budget import Budget, digest
from a1_process import output
from a1_run import HERE, REPO, execution_core, execution_inventory, read, seal, write
from a2_reference import checkpoint
from a2_sampling import validate_alignment, validate_payments, validate_snapshot
from a3_freeze import CONTROL, CONTROL_SHA, same_reference_inputs, validate_files
from a3_identity import validate_recovered

STOP = "FAIL: missing or invalid evidence: 0.2493150684931507"


def repairable(state):
    if (
        state.get("a3_stop_reason") != STOP
        or state["active_phase"] is not None
        or state["paths"] != [16, 0]
        or state["paths_completed"] != [0, 0]
        or state.get("a3_repair")
    ):
        raise RuntimeError(
            "Only the sealed decimal-time controller failure is repairable"
        )


def repaired_core(root, old, original, binaries):
    sources, paths = execution_inventory(binaries, original)
    actual = execution_core(paths, original, binaries)
    previous = old["core"]
    for path, expected in previous["files"].items():
        if path.startswith(str(binaries)) and digest(path) != expected:
            raise RuntimeError("Controller repair cannot change a native binary/build")
    for key in ("dependency_pins", "compiler", "python", "packages"):
        if actual[key] != previous[key]:
            raise RuntimeError("Controller repair dependency mismatch")
    same_reference_inputs(
        read(root / "a3/request.json"), read(root / "a2/request.json")
    )
    core = dict(previous, files=actual["files"], source_head=actual["source_head"])
    core["validation_repair"] = dict(
        original_manifest_sha256=digest(root / "a3/manifest.json"),
        original_execution_identity=old["identity"],
        previous_ledger_sha256=digest(root / "ledger.json"),
        previous_journal_sha256=digest(root / "ledger.jsonl"),
        sequence=read(root / "ledger.json")["sequence"],
        raw_sha256=digest(root / "a3/F2_linear-alignment.json"),
        checkpoint_sha256=digest(root / "a3/F2_linear-alignment.json.checkpoint.json"),
        defect="Canonical decimal request time string mismatched binary64 history key",
        numerical_change=False,
        original_stop=STOP,
    )
    return sources, core


def append_repair(budget, identity, evidence):
    repairable(budget.state)
    budget.state["a3_repair"] = dict(evidence, new_identity=identity)
    budget.save("disposition:A/3-payment-key")
    budget.state["identity"] = identity
    budget.state["a3_stop_reason"] = None
    budget.save("transition:A/3-payment-key")


def repair(root, original, binaries):
    if digest(CONTROL) != CONTROL_SHA or output(
        "git", ["-C", str(REPO), "status", "--porcelain"]
    ):
        raise RuntimeError(
            "Controller repair requires clean committed source and frozen control"
        )
    old = read(root / "a3/manifest.json")
    budget = validate_recovered(root, old)
    repairable(budget.state)
    directory = root / "a3"
    raw = read(directory / "F2_linear-alignment.json")
    validate_alignment(raw)
    validate_snapshot(directory, "F2_linear", raw["snapshot"])
    validate_payments(directory, "F2_linear", raw)
    sources, core = repaired_core(root, old, original, binaries)
    identity = seal(core)
    manifest = dict(old, core=core, identity=identity)
    target = directory / "payment-key-repair"
    target.mkdir()
    for name in ("ledger.json", "ledger.jsonl"):
        (target / name).write_bytes((root / name).read_bytes())
    for source in sources:
        saved = target / "executed-source" / source.relative_to(HERE)
        saved.parent.mkdir(parents=True, exist_ok=True)
        saved.write_bytes(source.read_bytes())
    write(target / "manifest.json", manifest)
    append_repair(budget, identity, core["validation_repair"])
    write(target / "transition-state.json", budget.state)
    path = directory / "F2_linear-alignment.json"
    prior = str(path) + ".checkpoint.json"
    budget.state.setdefault("alignment", {})["0"] = digest(path)
    budget.state.setdefault("mc_checkpoints", {})["0"] = dict(
        path=prior,
        sha256=digest(prior),
        tiny=True,
        evidence_sha256=digest(path),
        native_execution_identity=old["identity"],
        validation_execution_identity=identity,
    )
    budget.completed_paths(0, 16)
    checkpoint(
        directory,
        budget,
        [path.name, path.name + ".checkpoint.json"],
        "revalidate-existing-native-tiny",
    )
    print(
        "A/3 controller repair",
        identity,
        "preserved native tiny; no new fields",
        flush=True,
    )


def active_manifest(root, original):
    target = root / "a3/payment-key-repair"
    if not target.exists():
        return original
    manifest = read(target / "manifest.json")
    core = manifest["core"]
    if seal(core) != manifest["identity"] or core["control_sha256"] != CONTROL_SHA:
        raise RuntimeError("Controller repair execution seal mismatch")
    evidence = core["validation_repair"]
    checks = {
        root / "a3/manifest.json": evidence["original_manifest_sha256"],
        target / "ledger.json": evidence["previous_ledger_sha256"],
        target / "ledger.jsonl": evidence["previous_journal_sha256"],
        root / "a3/F2_linear-alignment.json": evidence["raw_sha256"],
        root / "a3/F2_linear-alignment.json.checkpoint.json": evidence[
            "checkpoint_sha256"
        ],
    }
    if any(digest(p) != expected for p, expected in checks.items()):
        raise RuntimeError("Controller repair origin or native evidence changed")
    validate_origin(original, evidence)
    budget = Budget(root / "ledger.json", manifest["identity"])
    prefix = (target / "ledger.jsonl").read_bytes()
    if not budget.journal.read_bytes().startswith(prefix):
        raise RuntimeError("Controller repair journal prefix changed")
    if budget.state["a3_repair"] != dict(evidence, new_identity=manifest["identity"]):
        raise RuntimeError("Controller repair state binding mismatch")
    validate_files(root, manifest)
    return manifest


def validate_origin(original, evidence):
    if evidence["original_execution_identity"] != original["identity"]:
        raise RuntimeError("Controller repair original identity mismatch")
    if evidence["numerical_change"] or evidence["original_stop"] != STOP:
        raise RuntimeError("Controller repair cannot dispose a numerical failure")

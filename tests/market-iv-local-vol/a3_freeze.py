"""Freeze A/3 source, exact recovery mapping and field-level reference reuse."""

from a1_budget import Budget, digest
from a1_process import output, program
from a1_run import (
    CONTROL,
    HERE,
    REPO,
    execution_core,
    execution_inventory,
    read,
    seal,
    write,
)
from a2_freeze import fixed_root
from a2_reference import validate_checkpoint
from a3_identity import (
    A2_MANIFEST_SHA,
    APPROVAL,
    BOUNDARY,
    CONTROL_SHA,
    OLD_CONTROL,
    recover,
    replacement_map,
    validate_recovered,
    validate_replacements,
    verify_boundary,
)

REUSED = (
    "history.json",
    "grid-studies.json",
    "stage1-admission.json",
    "n4-summary.json",
    "interpolation-probes.json",
)


def same_reference_inputs(old, new):
    before = {k: v for k, v in old.items() if k != "protocol"}
    after = {k: v for k, v in new.items() if k != "protocol"}
    if before != after:
        raise RuntimeError("A/3 may change only the request MC protocol")
    return {k: seal(old[k]) for k in before}


def reference_dependencies(root, core):
    old = read(root / "a2/manifest.json")["core"]
    for key in ("dependency_pins", "compiler", "python", "packages"):
        if core[key] != old[key]:
            raise RuntimeError("Reference reuse toolchain mismatch: " + key)
    names = (
        "a1_pde.py",
        "pde.py",
        "reference.py",
        "a1_study.py",
        "qualify.py",
        "grid_study.py",
        "a1_n4.py",
        "mcpayoff.hpp",
    )
    result = {}
    for name in names:
        expected = digest(root / "a2/executed-source" / name)
        if digest(HERE / name) != expected:
            raise RuntimeError("Reference dependency changed: " + name)
        result[name] = expected
    for suffix in (
        "workspace/dal-cpp/libdal_cpp.a",
        "dal_market_iv_capture",
        "dal_market_iv_a1_probe",
        "dal_market_iv_mc",
    ):
        path = program("capture").parent / suffix
        if digest(path) != old["files"][str(path)]:
            raise RuntimeError("Reference/production dependency changed: " + suffix)
        result[suffix] = digest(path)
    return result


def reuse_evidence(root, budget, input_hashes):
    validate_checkpoint(root / "a2", budget)
    names = set(budget.state["a2_files"]) | {
        "n4-nodes.jsonl",
        "probe-input.txt",
        "probe-actual.txt",
        "history.json",
    }
    return [
        dict(
            path="a2/" + name,
            sha256=digest(root / "a2" / name),
            actual_execution_identity=BOUNDARY["identity"],
            input_fields=input_hashes,
            dependency_set="reference-v1",
            change_intersection=[],
            reason="Only MC protocol changes; retain original reference identity",
        )
        for name in sorted(names)
    ]


def failure_evidence(root):
    paths = [
        root / "a2/F2_linear-alignment.log",
        root / "a2/F2_linear-alignment.json",
        root / "a2/executed-source/a1_mc.cpp",
        root / "a2/executed-source/mcpayoff.hpp",
    ]
    if "Production introduced a non-nested internal step" not in paths[0].read_text():
        raise RuntimeError("Missing original pre-fine-RNG failure evidence")
    source = paths[2].read_text()
    old_files = read(root / "a2/manifest.json")["core"]["files"]
    for path in paths[2:]:
        if digest(path) != old_files[str(HERE / path.name)]:
            raise RuntimeError("Original pre-fine-RNG source snapshot changed")
    start = source.index("Sums_ Batch(")
    if source.index("make_unique<Level_>", start) >= source.index(
        "auto rng = New", start
    ):
        raise RuntimeError("Original failure is not proven before fine RNG creation")
    return {str(p.relative_to(root)): digest(p) for p in paths}


def execution_extension(root, original, binaries):
    if digest(root / "a2/manifest.json") != A2_MANIFEST_SHA:
        raise RuntimeError("Sealed A/2 manifest cannot be rebound")
    old_manifest = read(root / "a2/manifest.json")
    if seal(old_manifest["core"]) != BOUNDARY["identity"]:
        raise RuntimeError("Original A/2 execution seal mismatch")
    old_budget = Budget(root / "ledger.json", BOUNDARY["identity"])
    sources, paths = execution_inventory(binaries, original)
    core = execution_core(paths, original, binaries)
    new_request = read(HERE / "a3/request.json")
    old_request = read(root / "a2/request.json")
    inputs = same_reference_inputs(old_request, new_request)
    evidence = failure_evidence(root)
    context = dict(
        old_identity=BOUNDARY["identity"],
        old_control=OLD_CONTROL,
        new_control=CONTROL_SHA,
        old_request=digest(root / "a2/request.json"),
        new_request=digest(HERE / "a3/request.json"),
        old_family=seal(old_request["protocol"]["families"]),
        new_family=seal(new_request["protocol"]["families"]),
        old_slots=seal(old_request["protocol"]),
        new_slots=seal(new_request["protocol"]),
        evidence=seal(evidence),
        approval=APPROVAL,
        old_source=old_manifest["core"]["source_head"],
        new_source=core["source_head"],
        old_binary=old_manifest["core"]["files"][str(binaries / "dal_market_iv_a1_mc")],
        new_binary=digest(binaries / "dal_market_iv_a1_mc"),
    )
    core.update(
        schema="dal.market-iv-a3-execution/1",
        control_sha256=CONTROL_SHA,
        protocol=new_request["protocol"],
        approval=APPROVAL,
        boundary=BOUNDARY,
        recovery_context=context,
        failure_evidence=evidence,
        reference_inputs=inputs,
        reference_dependencies=reference_dependencies(root, core),
        reference_reuse=reuse_evidence(root, old_budget, inputs),
        old_manifest_sha256=digest(root / "a2/manifest.json"),
        maximum_foreground_seconds=21600,
        default_foreground_seconds=21600,
        gate_order=[
            "reuse-reference-grid",
            "actual-protocol-default-tiny-both-families",
            "formal-SE-only-MC",
        ],
    )
    return sources, core


def freeze_a3(root, original, binaries):
    fixed_root(root)
    verify_boundary(root, BOUNDARY)
    if (root / "a3").exists():
        raise RuntimeError("A/3 namespace already exists; cannot repeat recovery")
    if digest(CONTROL) != CONTROL_SHA or output(
        "git", ["-C", str(REPO), "status", "--porcelain"]
    ):
        raise RuntimeError("A/3 requires committed source and matching frozen control")
    if binaries != program("capture").parent:
        raise RuntimeError("A/3 actual binary namespace mismatch")
    sources, core = execution_extension(root, original, binaries)
    identity = seal(core)
    context = dict(core["recovery_context"], new_identity=identity)
    rows = replacement_map(context)
    directory = root / "a3"
    directory.mkdir()
    (directory / "request.json").write_bytes((HERE / "a3/request.json").read_bytes())
    for name in REUSED:
        (directory / name).write_bytes((root / "a2" / name).read_bytes())
    for source in sources:
        saved = directory / "executed-source" / source.relative_to(HERE)
        saved.parent.mkdir(parents=True, exist_ok=True)
        saved.write_bytes(source.read_bytes())
    manifest = dict(core=core, identity=identity, context=context, replacements=rows)
    write(directory / "manifest.json", manifest)
    budget = recover(root, identity, BOUNDARY, context, rows)
    if budget.state["sequence"] != 2286:
        raise RuntimeError("Expected disposition 2285 and transition 2286")
    print("A/3 frozen", identity, "disposition=2285 transition=2286", flush=True)


def validate_a3(root):
    fixed_root(root)
    manifest = read(root / "a3/manifest.json")
    core = manifest["core"]
    if seal(core) != manifest["identity"] or core["control_sha256"] != CONTROL_SHA:
        raise RuntimeError("A/3 frozen identity mismatch")
    expected_context = dict(core["recovery_context"], new_identity=manifest["identity"])
    if manifest["context"] != expected_context:
        raise RuntimeError("A/3 recovery context mismatch")
    validate_replacements(manifest["replacements"], expected_context, [16, 0], [0, 0])
    validate_files(root, manifest)
    budget = validate_recovered(root, manifest)
    validate_checkpoint(root / "a3", budget)
    return manifest, budget


def validate_files(root, manifest):
    for name, expected in manifest["core"]["files"].items():
        if digest(name) != expected:
            raise RuntimeError(
                "A/3 actual source/build/binary identity mismatch: " + name
            )
    if digest(root / "a3/request.json") != manifest["context"]["new_request"]:
        raise RuntimeError("A/3 runtime request mismatch")
    if digest(root / "a2/manifest.json") != manifest["core"]["old_manifest_sha256"]:
        raise RuntimeError("Original A/2 manifest cannot be rebound")
    for row in manifest["core"]["reference_reuse"]:
        if digest(root / row["path"]) != row["sha256"]:
            raise RuntimeError("Reused evidence changed: " + row["path"])

"""Separate small alignment admission from formal cumulative-prefix sampling."""

import math
import time
from pathlib import Path

from a1_budget import digest
from a1_run import command, read
from a1_verify import batch_statistics, verify
from a2_reference import checkpoint
from a3_protocol import DIMENSIONS, DMAX


def validate_tiny_identity(value):
    if (
        value["sampling"] != "SMOKE_NOT_QUALIFIED"
        or value["dmax"] != DMAX
        or value["seed"] != 1024
    ):
        raise RuntimeError("Default alignment sampling identity mismatch")
    if value["tiers"][0]["paths_per_batch"] != 1:
        raise RuntimeError("Alignment must retain only the registered tiny prefix")
    if value["previous_paths_per_batch"] != 0 or value["new_paths_per_batch"] != 1:
        raise RuntimeError("Tiny must generate exactly its unexecuted first field")
    if len(value["tiers"][0]["groups"]) != 14:
        raise RuntimeError("Missing tiny paired-output groups")


def validate_actual_models(value):
    admission = value["protocol_admission"]
    if admission["status"] != "PASS" or admission["brownian_pairing"] != "PASS":
        raise RuntimeError("Missing full actual protocol admission")
    if (
        len(admission["models"]) != 8
        or admission["last_scalar_address"] != 293131517951
    ):
        raise RuntimeError("Missing complete eight-model or scalar-range evidence")
    for index, model in enumerate(admission["models"]):
        validate_model(model, index)


def validate_alignment(value):
    validate_tiny_identity(value)
    validate_actual_models(value)
    for group in value["tiers"][0]["groups"]:
        batch_statistics(group)


def validate_model(model, index):
    expected = dict(
        grid="P7" if index < 4 else "P8",
        level=index % 4,
        expected_dimension=DIMENSIONS[index % 4],
        actual_dimension=DIMENSIONS[index % 4],
        default_paths=64,
        status="PASS",
    )
    if any(model.get(k) != v for k, v in expected.items()) or not math.isfinite(
        model["default_price"]
    ):
        raise RuntimeError(
            "Actual default model/coordinate/path/price evidence mismatch"
        )


def align_family(directory, manifest, budget, family, name):
    saved = budget.state.get("alignment", {}).get(str(family))
    path = directory / (name + "-alignment.json")
    if saved:
        if digest(path) != saved:
            raise RuntimeError("Alignment checkpoint hash mismatch")
        return
    phase = "mc" + str(family)
    binary = Path(manifest["core"]["binary_directory"]) / "dal_market_iv_a1_mc"
    budget.begin(phase)
    budget.paths(family, 16)
    command(
        [str(binary), str(directory / "request.json"), str(path), name, "0"],
        directory,
        budget,
        phase,
        name + "-alignment.log",
    )
    validate_alignment(read(path))
    validate_snapshot(directory, name, read(path)["snapshot"])
    validate_payments(directory, name, read(path))
    budget.completed_paths(family, 16)
    prior = str(path) + ".checkpoint.json"
    budget.state.setdefault("mc_checkpoints", {})[str(family)] = {
        "path": prior,
        "sha256": digest(prior),
        "tiny": True,
        "evidence_sha256": digest(path),
    }
    budget.state.setdefault("alignment", {})[str(family)] = digest(path)
    budget.end(phase)
    checkpoint(
        directory,
        budget,
        [path.name, path.name + ".checkpoint.json"],
        name + ":alignment",
    )


def validate_snapshot(directory, name, actual):
    capture = read(directory.parent / "a2/capture.json")
    expected = next(f["snapshot"] for f in capture["fixtures"] if f["id"] == name)
    if actual != expected:
        raise RuntimeError(
            "New actual MC fitted snapshot differs from reused reference input"
        )


def payment_targets(directory, name):
    history = read(directory / "history.json")
    products = {"discount": 10, "fixed_payment": 18, "delayed_payment": 19}
    cases = read(directory / "request.json")["fixtures"]
    fixture = next(f for f in cases if f["id"] == name)
    by_time = {float(c["time"]): i for i, c in enumerate(fixture["mc_cases"])}
    return {
        20 * by_time[r["maturity"]] + products[r["product"]]: r["oracle"]
        for r in history["rows"]
        if r["fixture"] == name and r["product"] in products
    }


def validate_payments(directory, name, value):
    targets = payment_targets(directory, name)
    for group in value["tiers"][0]["groups"][:8]:
        for batch in group["batch_means"]:
            for index, expected in targets.items():
                if abs(batch[index] - expected) > 2e-12:
                    raise RuntimeError("FAIL: tiny deterministic DF/payment gate")


def align_families(directory, manifest, budget):
    for family, name in enumerate(("F2_linear", "F2_nonlinear")):
        align_family(directory, manifest, budget, family, name)


def tiny_prefix(budget, family):
    allowed = 16 if str(family) in budget.state.get("alignment", {}) else 0
    if budget.state["paths"][family] != allowed:
        raise RuntimeError("Unaccounted prefix without formal checkpoint")
    if budget.state["paths_completed"][family] != allowed:
        raise RuntimeError("The tiny prefix has not actually completed")
    if allowed:
        prior = budget.state["mc_checkpoints"][str(family)]
        if not prior.get("tiny") or digest(prior["path"]) != prior["sha256"]:
            raise RuntimeError("Tiny checkpoint hash mismatch")
        return None, Path(prior["path"]), 1
    return None, None, 0


def restore_family(directory, budget, family, name):
    path = directory / (name + "-mc.json")
    if not path.exists():
        return tiny_prefix(budget, family)
    value = read(path)
    verify(value)
    prior = budget.state["mc_checkpoints"][str(family)]
    if digest(path) != prior["evidence_sha256"]:
        raise RuntimeError("Formal estimates checkpoint hash mismatch")
    checkpoint_path = Path(prior["path"])
    previous = value["tiers"][-1]["paths_per_batch"]
    if (
        digest(checkpoint_path) != prior["sha256"]
        or budget.state["paths_completed"][family] != 16 * previous
    ):
        raise RuntimeError("Formal checkpoint prefix identity mismatch")
    return value, checkpoint_path, previous


def check_front_window(budget, paths, previous):
    required = 21600 * (paths - previous) / 2**20 + 30
    remaining = getattr(budget, "foreground_deadline", float("inf")) - time.monotonic()
    if remaining < required:
        raise RuntimeError("INCOMPLETE: foreground window before next tier")

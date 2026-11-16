"""Separate small alignment admission from formal cumulative-prefix sampling."""

import math
import time
from pathlib import Path

from a1_budget import digest
from a1_run import command, read
from a1_verify import verify
from a2_reference import checkpoint


def validate_alignment(value):
    if (
        value["sampling"] != "SMOKE_NOT_QUALIFIED"
        or value["dmax"] != 8728
        or value["seed"] != 1024
    ):
        raise RuntimeError("Default alignment sampling identity mismatch")
    if value["tiers"][0]["paths_per_batch"] != 1:
        raise RuntimeError("Alignment must retain only the registered tiny prefix")
    if not all(
        math.isfinite(value[k])
        for k in ("P7_default_alignment", "P8_default_alignment")
    ):
        raise RuntimeError("Missing actual default alignment evidence")


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
    budget.completed_paths(family, 16)
    budget.state.setdefault("alignment", {})[str(family)] = digest(path)
    budget.end(phase)
    checkpoint(directory, budget, [path.name], name + ":alignment")


def align_families(directory, manifest, budget):
    for family, name in enumerate(("F2_linear", "F2_nonlinear")):
        align_family(directory, manifest, budget, family, name)


def restore_family(directory, budget, family, name):
    path = directory / (name + "-mc.json")
    if not path.exists():
        allowed = 16 if str(family) in budget.state.get("alignment", {}) else 0
        if budget.state["paths"][family] != allowed:
            raise RuntimeError("Unaccounted prefix without formal checkpoint")
        return None, None, 0
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

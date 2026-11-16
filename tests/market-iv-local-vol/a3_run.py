"""A/3 actual protocol admission and cumulative SE-only MC continuation."""

import time
from collections import Counter

from a1_report import report
from a1_run import mc_stage, read, write
from a2_reference import checkpoint
from a2_run import end_phase
from a2_sampling import align_families
from a3_freeze import freeze_a3, validate_a3

__all__ = ["freeze_a3", "run_a3"]


def protocol_admission(directory, manifest, budget):
    if "mc-admission.json" in budget.state.get("a3_files", {}):
        return
    align_families(directory, manifest, budget)
    if set(budget.state.get("alignment", {})) != {"0", "1"}:
        raise RuntimeError("Both actual family alignments are required")
    write(
        directory / "mc-admission.json",
        dict(
            status="PASS",
            amendment="A/3",
            reference_actual_identity=manifest["core"]["boundary"]["identity"],
            protocol=manifest["context"]["new_slots"],
            actual_alignments=budget.state["alignment"],
            actual_execution_identity=manifest["identity"],
        ),
    )
    checkpoint(directory, budget, ["mc-admission.json"], "A/3-protocol-admission")


def require_resumable(budget):
    prior = budget.state.get("a3_stop_reason")
    if prior and not prior.startswith("INCOMPLETE: foreground window"):
        raise RuntimeError(
            "Actual A/3 gate/resource failure requires disposition: " + prior
        )
    if budget.state["active_phase"] or budget.state.get("a3_complete"):
        raise RuntimeError("A/3 phase is unclosed or already complete")


def family_complete(directory, name):
    path = directory / (name + "-mc.json")
    return path.exists() and read(path)["tiers"][-1]["se_sufficient"]


def qualify_rows(directory, result, manifest):
    for row in result["rows"]:
        if row["fixture"].startswith("F2"):
            row["evidence_namespace"] = "A/3"
            row["reference_actual_identity"] = manifest["core"]["boundary"]["identity"]
            row["formal_family_complete"] = family_complete(directory, row["fixture"])
            if row["estimate"] is not None and not row["formal_family_complete"]:
                row["status"] = "INCOMPLETE"
            row["mc_status"] = "NOT_RUN" if row["estimate"] is None else row["status"]
    result["counts"] = dict(Counter(r["status"] for r in result["rows"]))


def publish(directory, manifest, budget, reason):
    studies = read(directory / "grid-studies.json")
    lookup = {(r["fixture"], r["grid"], r["maturity"]): r for r in studies}
    result = report(directory, read(directory / "history.json"), lookup, reason)
    qualify_rows(directory, result, manifest)
    result.update(
        execution_identity=manifest["identity"], amendment="A/3", budget=budget.state
    )
    write(
        directory / ("qualification-" + str(budget.state["sequence"]) + ".json"), result
    )
    print(
        "A/3 RESULT",
        dict(
            reason=reason,
            counts=result["counts"],
            accepted=result["accepted"],
            solves=budget.state["solves_completed"],
            paths=budget.state["paths_completed"],
            seconds=budget.state["seconds"],
            total_seconds=budget.state["total_seconds"],
        ),
        flush=True,
    )
    return 0 if result["accepted"] else 2


def run_a3(root, window_seconds=21600):
    manifest, budget = validate_a3(root)
    require_resumable(budget)
    if not 0 < window_seconds <= manifest["core"]["maximum_foreground_seconds"]:
        raise RuntimeError("A/3 foreground window exceeds registered operational bound")
    budget.foreground_deadline = time.monotonic() + window_seconds
    budget.state["foreground_seconds"] = window_seconds
    budget.save("foreground-window:A/3")
    reason = None
    try:
        protocol_admission(root / "a3", manifest, budget)
        mc_stage(root / "a3", manifest, budget)
    except (RuntimeError, ValueError) as error:
        reason = str(error)
    except (KeyError, TypeError) as error:
        reason = "FAIL: missing or invalid evidence: " + str(error)
    finally:
        end_phase(budget)
    budget.state["a3_stop_reason"] = reason
    budget.state["a3_complete"] = reason is None
    budget.save("A/3:stopped" if reason else "A/3:complete")
    return publish(root / "a3", manifest, budget, reason)

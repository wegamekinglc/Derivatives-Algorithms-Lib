"""Execute A/2 admission in the frozen order using the same source ledger."""

import time

from a1_run import capture_stage, mc_stage, persist_study, read, stage2, write
from a1_study import certify
from a2_freeze import freeze_a2, validate_a2
from a2_reference import (
    LayerRunner,
    arguments,
    black_cases,
    checkpoint,
    old_cases,
)

__all__ = ["freeze_a2", "run_a2"]


def cases(root):
    return list(black_cases(read(root / "request.json"))) + list(
        old_cases(read(root / "old-capture.json"))
    )


def revalidate_expanded(root, directory, runner, mapping):
    by_name = {c[0]: c for c in cases(root)}
    selected = [row for row in mapping if row["action"] == "P3_RECOMPUTE"]
    for row in selected:
        name = row["study"]
        runner(arguments(by_name[name], [tuple(row["setting"])]), name)
    write(directory / "P3-expanded-admission.json", dict(status="COMPLETE", layers=10))


def certify_case(case, directory, runner, budget, diagnostic):
    name, model, time_value, knots, black_limit = case
    result = certify(name, model, time_value, knots, runner, black_limit)
    result["diagnostic"] = diagnostic
    write(directory / (name + ".json"), result)
    checkpoint(directory, budget, [name + ".json"], name + ":study")
    if diagnostic:
        print(name, "DIAGNOSTIC", result["status"], flush=True)
    else:
        persist_study(directory, name, result)
    return result


def reference_admission(root, directory, manifest, runner, budget):
    budget.begin("pde")
    revalidate_expanded(root, directory, runner, manifest["core"]["reuse_plan"])
    all_cases = cases(root)
    for case in all_cases[:8]:
        certify_case(case, directory, runner, budget, False)
    for case in all_cases[8:]:
        certify_case(case, directory, runner, budget, True)
    budget.end("pde")
    write(
        directory / "stage1-admission.json",
        dict(
            status="PASS",
            black_studies=8,
            old_diagnostics=8,
            old_unavailability_nonblocking=True,
        ),
    )
    checkpoint(directory, budget, ["stage1-admission.json"], "reference-admission")


def finished_stage(directory, budget, name):
    relative = name + ".json"
    return (
        relative in budget.state.get("a2_files", {}) and (directory / relative).exists()
    )


def resume_stages(root, directory, manifest, budget):
    runner = LayerRunner(root, directory, budget, manifest["core"]["reuse_plan"])
    if not finished_stage(directory, budget, "stage1-admission"):
        reference_admission(root, directory, manifest, runner, budget)
    if finished_stage(directory, budget, "capture-admission"):
        captured = read(directory / "capture.json")
    else:
        captured = capture_stage(directory, manifest, budget)
        write(directory / "capture-admission.json", dict(status="PASS"))
        checkpoint(
            directory,
            budget,
            [
                "capture-admission.json",
                "capture.json",
                "n4-summary.json",
                "interpolation-probes.json",
            ],
            "capture",
        )
    if not finished_stage(directory, budget, "mc-admission"):
        stage2(directory, captured, budget, runner)
        checkpoint(
            directory, budget, ["mc-admission.json", "grid-studies.json"], "grid"
        )
    from a2_sampling import align_families

    align_families(directory, manifest, budget)
    mc_stage(directory, manifest, budget)


def end_phase(budget):
    if not budget.state["active_phase"]:
        return
    try:
        budget.end(budget.state["active_phase"])
    except RuntimeError:
        budget.state["active_phase"] = None
        budget.save("stopped-at-limit")


def publish_result(directory, manifest, budget, reason):
    from a1_report import report

    path = directory / "grid-studies.json"
    studies = read(path) if path.exists() else []
    lookup = {(r["fixture"], r["grid"], r["maturity"]): r for r in studies}
    result = report(directory, read(directory / "history.json"), lookup, reason)
    for row in result["rows"]:
        if row["fixture"].startswith("F2"):
            row["evidence_namespace"] = "A/2"
            row["mc_status"] = "NOT_RUN" if row["estimate"] is None else row["status"]
    result.update(
        execution_identity=manifest["identity"], budget=budget.state, amendment="A/2"
    )
    path = directory / ("qualification-" + str(budget.state["sequence"]) + ".json")
    write(path, result)
    print("A/2 RESULT", reason, result["counts"], budget.state, flush=True)
    return 0 if result["accepted"] else 2


def require_resumable(budget):
    prior = budget.state.get("a2_stop_reason")
    if prior and not prior.startswith("INCOMPLETE: foreground window"):
        raise RuntimeError(
            "Prior genuine gate/resource stop requires disposition: " + prior
        )
    if budget.state["active_phase"]:
        raise RuntimeError(
            "Interrupted active phase requires explicit accounting recovery"
        )
    if budget.state.get("a2_complete"):
        raise RuntimeError("Completed A/2 execution cannot be repeated")


def run_a2(root, window_seconds=1800):
    manifest, budget = validate_a2(root)
    require_resumable(budget)
    if not 0 < window_seconds <= manifest["core"]["maximum_foreground_seconds"]:
        raise RuntimeError("Foreground window exceeds frozen operational bound")
    budget.foreground_deadline = time.monotonic() + window_seconds
    budget.state["foreground_seconds"] = window_seconds
    budget.save("foreground-window")
    reason = None
    try:
        resume_stages(root, root / "a2", manifest, budget)
    except (RuntimeError, ValueError) as error:
        reason = str(error)
    finally:
        end_phase(budget)
    budget.state["a2_stop_reason"] = reason
    budget.state["a2_complete"] = reason is None
    budget.save("A/2:stopped" if reason else "A/2:complete")
    return publish_result(root / "a2", manifest, budget, reason)

"""Freeze one A/1 execution and run its sequential numerical admission stages."""

import argparse
import hashlib
import json
import os
import platform
import time
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, wait
from functools import partial
from importlib.metadata import version
from pathlib import Path

from a1_axes import MATURITIES
from a1_budget import LIMITS, Budget, digest
from a1_n4 import nodes, probes
from a1_process import launch, native_kind, output
from a1_report import report
from a1_study import (
    black_model,
    canonical_logs,
    certify,
    grid_comparison,
    make_model,
    task,
)
from a1_verify import verify

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
CONTROL = REPO / ".codex/artifacts/specs/dal-542-n5-resolution-a.md"
CONTROL_SHA = "acf410567786916601584b71a954991b0a2c81f180aab734cd4be0091ed96ec7"
BINARIES = (
    "dal_market_iv_capture",
    "dal_market_iv_mc",
    "dal_market_iv_a1_mc",
    "dal_market_iv_a1_probe",
    "dal_market_iv_a1_protocol_tests",
)


def write(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def read(path):
    return json.loads(path.read_text())


def seal(value):
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def execution_inventory(binaries, original):
    sources = [
        p for p in HERE.rglob("*") if p.is_file() and "__pycache__" not in p.parts
    ]
    executable = [binaries / name for name in BINARIES]
    controls = [
        CONTROL,
        REPO / ".codex/artifacts/api-notes/dal-538-market-iv-local-vol-mc.md",
        REPO / ".codex/artifacts/plans/dal-538-market-iv-local-vol-mc.md",
    ]
    paths = (
        sources
        + executable
        + controls
        + [
            binaries / "CMakeCache.txt",
            binaries / "compile_commands.json",
            binaries / "workspace/dal-cpp/libdal_cpp.a",
            original / "capture.json",
            original / "formal-qualification.json",
        ]
    )
    return sources, paths


def execution_core(paths, original, binaries):
    core = dict(
        schema="dal.market-iv-a1-execution/1",
        control_sha256=CONTROL_SHA,
        source_head=output("git", ["-C", str(REPO), "rev-parse", "HEAD"]).strip(),
        files={str(p.resolve()): digest(p) for p in paths},
        dependency_pins=output(
            "git", ["-C", str(REPO), "submodule", "status", "--recursive"]
        ),
        compiler=output("compiler", ["--version"]),
        python=platform.python_version(),
        packages={name: version(name) for name in ("numpy", "scipy", "mpmath")},
        platform=platform.platform(),
        cpu_count=os.cpu_count(),
        binary_directory=str(binaries.resolve()),
        limits=LIMITS,
        old_history_counts=read(original / "formal-qualification.json")["counts"],
        protocol=read(HERE / "a1/request.json")["protocol"],
        outputs=["call"] * 5
        + ["put"] * 5
        + ["discount", "discounted_forward"]
        + ["parity"] * 5
        + ["forward_mean", "fixed_payment", "delayed_payment"],
        reference_order=[
            "Black_knots",
            "Black_flat",
            "F2_linear_old_tau_quarter",
            "F2_nonlinear_old_tau_quarter",
        ],
        reference_phase2=["P7", "P8"],
        diagnostics=["P" + str(i) for i in range(7)],
        requested_steps=[512, 1024, 2048, 4096],
        central_nodes=[601, 1201, 2401, 4801],
        expanded_nodes=[801, 1601, 3201, 6401],
        workers=dict(pde=4, mc=16),
        parser="RapidJSON kParseFullPrecisionFlag; Python binary64 round-trip",
        gate_order=[
            "support",
            "stage1_reference",
            "capture_N4",
            "stage2_reference_grid",
            "diagnostics",
            "MC",
        ],
    )
    return core


def save_namespace(directory, original, sources):
    for source, name in (
        (HERE / "a1/request.json", "request.json"),
        (original / "capture.json", "old-capture.json"),
        (original / "formal-qualification.json", "history.json"),
    ):
        (directory / name).write_bytes(source.read_bytes())
    for source in sources:
        saved = directory / "executed-source" / source.relative_to(HERE)
        saved.parent.mkdir(parents=True, exist_ok=True)
        saved.write_bytes(source.read_bytes())


def freeze(directory, original, binaries):
    raise RuntimeError(
        "A/2 requires freeze-a2 on the existing source budget; no new budget"
    )


def validate(directory):
    manifest = read(directory / "manifest.json")
    if seal(manifest["core"]) != manifest["identity"]:
        raise RuntimeError("Execution manifest seal mismatch")
    for name, value in manifest["core"]["files"].items():
        if digest(name) != value:
            raise RuntimeError("Frozen execution identity mismatch: " + name)
    for name, value in manifest["namespace_files"].items():
        if digest(directory / name) != value:
            raise RuntimeError("Frozen namespace input mismatch: " + name)
    budget = Budget(directory / "ledger.json", manifest["identity"])
    return manifest, budget


def stop_children(processes):
    status = json.loads(output("multica", ["daemon", "status", "--output", "json"]))

    def pids(value):
        if not isinstance(value, dict):
            return set()
        result = {
            v for k, v in value.items() if "pid" in k.lower() and isinstance(v, int)
        }
        for v in value.values():
            result.update(pids(v))
        return result

    forbidden = pids(status)
    for process in processes:
        if process.pid in forbidden:
            raise RuntimeError("Refusing to terminate daemon PID")
        process.terminate()


def wait_results(executor, futures, directory, budget):
    pending = set(futures)
    while pending:
        done, pending = wait(pending, timeout=1, return_when=FIRST_COMPLETED)
        budget.tick("pde")
        budget.resources(directory, [p.pid for p in executor._processes.values()])
        for future in done:
            future.result()
            budget.completed()


def execute(arguments, name, directory, budget):
    budget.check_capacity(len(arguments))
    for argument in arguments:
        budget.reserve_solve(name + ":" + repr(argument[-1]))
    executor = ProcessPoolExecutor(max_workers=4)
    futures = [executor.submit(task, arg) for arg in arguments]
    try:
        wait_results(executor, futures, directory, budget)
        return [future.result() for future in futures]
    except BaseException:
        stop_children(list(executor._processes.values()))
        raise
    finally:
        executor.shutdown(wait=True, cancel_futures=True)


def command(arguments, directory, budget, phase, logfile):
    with (directory / logfile).open("w") as stream:
        process = launch(native_kind(arguments[0]), arguments[1:], stream)
        try:
            while process.poll() is None:
                time.sleep(0.5)
                budget.tick(phase)
                budget.resources(directory, [process.pid])
            if process.returncode:
                raise RuntimeError(
                    f"FAIL: {Path(arguments[0]).name} exit {process.returncode}"
                )
        finally:
            if process.poll() is None:
                stop_children([process])
            process.wait()


def persist_study(directory, name, result):
    write(directory / (name + ".json"), result)
    maxima = {k: max(v) for k, v in result["normalized_differences"].items()}
    print(
        json.dumps(
            dict(
                study=name,
                status=result["status"],
                refined=result["refined"],
                maxima=maxima,
            )
        ),
        flush=True,
    )
    if result["status"] != "PASS":
        raise RuntimeError("REFERENCE_UNAVAILABLE: " + name)


def black_studies(directory, request, run):
    for flat in (False, True):
        model = black_model(request["grids"][7], flat)
        for i, time_value in enumerate(MATURITIES):
            name = f"Black_{'flat' if flat else 'knots'}_T{i}"
            result = certify(
                name,
                model,
                time_value,
                canonical_logs(request["grids"][7]["spots"]),
                run,
                True,
            )
            persist_study(directory, name, result)


def old_studies(directory, old, run):
    for fixture in old["fixtures"]:
        if not fixture["id"].startswith("F2"):
            continue
        grid = next(g for g in fixture["grids"] if g["id"] == "tau_quarter")
        model = make_model(fixture, grid)
        for i, time_value in enumerate(MATURITIES):
            name = fixture["id"] + "_old_T" + str(i)
            result = certify(
                name, model, time_value, canonical_logs(grid["spots"]), run
            )
            persist_study(directory, name, result)


def stage1(directory, request, old, budget):
    budget.begin("pde")
    run = partial(execute, directory=directory, budget=budget)
    black_studies(directory, request, run)
    old_studies(directory, old, run)
    budget.end("pde")
    write(directory / "stage1-admission.json", dict(status="PASS", studies=16))


def selected_runner(directory, budget, runner):
    return (
        runner
        if runner is not None
        else partial(execute, directory=directory, budget=budget)
    )


def stage2(directory, capture, budget, runner=None):
    continuous = [
        json.loads(line)
        for line in (HERE / "fixtures/pde.jsonl").read_text().splitlines()
    ]
    references = {(r["fixture"], r["maturity"]): r for r in continuous}
    records = {}
    budget.begin("pde")
    run = selected_runner(directory, budget, runner)
    for fixture in capture["fixtures"]:
        for grid in fixture["grids"]:
            if grid["id"] not in ("P7", "P8"):
                continue
            model = make_model(fixture, grid)
            for i, time_value in enumerate(MATURITIES):
                name = fixture["id"] + "_" + grid["id"] + "_T" + str(i)
                result = certify(
                    name, model, time_value, canonical_logs(grid["spots"]), run
                )
                result.update(fixture=fixture["id"], grid=grid["id"])
                result["comparison"] = grid_comparison(
                    result, references[(fixture["id"], time_value)]
                )
                records[(fixture["id"], grid["id"], time_value)] = result
                write(directory / "grid-studies.json", list(records.values()))
                persist_study(directory, name, result)
                if result["comparison"]["status"] != "PASS":
                    raise RuntimeError("FAIL: N5 grid " + name)
    diagnostics(directory, capture, run)
    budget.end("pde")
    write(
        directory / "mc-admission.json",
        dict(
            status="PASS",
            required_studies=16,
            grid_studies_sha256=digest(directory / "grid-studies.json"),
        ),
    )


def diagnostics(directory, capture, run):
    for fixture in capture["fixtures"]:
        for grid in fixture["grids"]:
            if grid["id"] in ("P7", "P8"):
                continue
            model = make_model(fixture, grid)
            for i, time_value in enumerate(MATURITIES):
                from a1_study import case_strikes

                name = fixture["id"] + "_" + grid["id"] + "_diagnostic_T" + str(i)
                values = run(
                    [
                        (
                            model,
                            time_value,
                            case_strikes(time_value),
                            canonical_logs(grid["spots"]),
                            (2, 2048, False),
                        )
                    ],
                    name,
                )
                write(
                    directory / (name + ".json"),
                    dict(status="INCOMPLETE", diagnostic=True, levels=values),
                )


def capture_stage(directory, manifest, budget):
    binaries = Path(manifest["core"]["binary_directory"])
    budget.begin("other")
    command(
        [
            str(binaries / "dal_market_iv_capture"),
            str(directory / "request.json"),
            str(directory / "capture.json"),
            "--full-precision",
        ],
        directory,
        budget,
        "other",
        "capture.log",
    )
    captured = read(directory / "capture.json")
    requested = read(directory / "request.json")
    old = {f["id"]: f for f in read(directory / "old-capture.json")["fixtures"]}
    for fixture in captured["fixtures"]:
        if fixture["snapshot"] != old[fixture["id"]]["snapshot"]:
            raise RuntimeError("FAIL: fitted snapshot identity changed")
        for actual, wanted in zip(fixture["grids"], requested["grids"]):
            if (
                not actual["success"]
                or actual["spots"] != wanted["spots"]
                or actual["times"] != wanted["times"]
            ):
                raise RuntimeError("FAIL: capture canonical coordinates")
    write(directory / "n4-summary.json", nodes(captured, directory, budget))
    write(
        directory / "interpolation-probes.json",
        probes(captured, directory, binaries / "dal_market_iv_a1_probe"),
    )
    budget.end("other")
    return captured


def require_family_pass(directory, name):
    studies = read(directory / "grid-studies.json")
    lookup = {(r["fixture"], r["grid"], r["maturity"]): r for r in studies}
    current = report(directory, read(directory / "history.json"), lookup, None)
    failed = next(
        (r for r in current["rows"] if r["fixture"] == name and r["status"] != "PASS"),
        None,
    )
    if failed:
        write(directory / "mc-failure.json", failed)
        raise RuntimeError("FAIL: completed MC family " + name)


def mc_stage(directory, manifest, budget):
    if read(directory / "mc-admission.json")["status"] != "PASS":
        raise RuntimeError("Missing reference/grid admission")
    binary = Path(manifest["core"]["binary_directory"]) / "dal_market_iv_a1_mc"
    for family, name in enumerate(("F2_linear", "F2_nonlinear")):
        if budget.state["paths"][family] != budget.state["paths_completed"][family]:
            raise RuntimeError(
                "Interrupted reserved MC tier requires explicit recovery"
            )
        phase = "mc" + str(family)
        budget.begin(phase)
        sample_family(directory, binary, budget, family, name, phase)
        budget.end(phase)
        require_family_pass(directory, name)


def sample_family(directory, binary, budget, family, name, phase):
    from a2_sampling import restore_family

    evidence, prior, previous = restore_family(directory, budget, family, name)
    if evidence and evidence["tiers"][-1]["se_sufficient"]:
        return
    start = 16 + (len(evidence["tiers"]) if evidence else 0)
    for exponent in range(start, 21):
        paths = 2**exponent
        from a2_sampling import check_front_window

        check_front_window(budget, paths, previous)
        budget.paths(family, 16 * paths)
        output = directory / (name + "-tier-" + str(exponent) + ".json")
        arguments = [
            str(binary),
            str(directory / "request.json"),
            str(output),
            name,
            str(exponent),
        ]
        if prior:
            checkpoint = budget.state["mc_checkpoints"][str(family)]
            if digest(prior) != checkpoint["sha256"]:
                raise RuntimeError("MC checkpoint hash mismatch")
            arguments.append(str(prior))
        command(
            arguments,
            directory,
            budget,
            phase,
            name + "-tier-" + str(exponent) + ".log",
        )
        evidence, prior, verification = record_sampling(
            directory, output, evidence, budget, family, name, paths
        )
        previous = paths
        if verification["tiers"][-1]["se_sufficient"]:
            return
    raise RuntimeError("INCOMPLETE: preregistered SE at path cap " + name)


def record_sampling(directory, output, evidence, budget, family, name, paths):
    current = read(output)
    if evidence:
        current["tiers"] = evidence["tiers"] + current["tiers"]
    verification = verify(current)
    write(directory / (name + "-mc.json"), current)
    write(directory / (name + "-sampling-verification.json"), verification)
    budget.completed_paths(family, 16 * paths)
    prior = Path(str(output) + ".checkpoint.json")
    budget.state.setdefault("mc_checkpoints", {})[str(family)] = {
        "path": str(prior),
        "sha256": digest(prior),
        "evidence_sha256": digest(directory / (name + "-mc.json")),
    }
    budget.save("checkpoint:" + name)
    from a2_reference import checkpoint

    checkpoint(directory, budget, [output.name, prior.name], name + ":tier")
    return current, prior, verification


def run(directory):
    manifest, budget = validate(directory)
    if (directory / "qualification.json").exists():
        raise RuntimeError(
            "Existing completed/checkpoint run cannot be silently repeated"
        )
    reason = None
    try:
        stage1(
            directory,
            read(directory / "request.json"),
            read(directory / "old-capture.json"),
            budget,
        )
        captured = capture_stage(directory, manifest, budget)
        stage2(directory, captured, budget)
        mc_stage(directory, manifest, budget)
    except (RuntimeError, ValueError) as error:
        reason = str(error)
        if budget.state["active_phase"]:
            try:
                budget.end(budget.state["active_phase"])
            except RuntimeError:
                budget.state["active_phase"] = None
                budget.save("stopped-at-limit")
    studies = (
        read(directory / "grid-studies.json")
        if (directory / "grid-studies.json").exists()
        else []
    )
    lookup = {(r["fixture"], r["grid"], r["maturity"]): r for r in studies}
    result = report(directory, read(directory / "history.json"), lookup, reason)
    result["execution_identity"] = manifest["identity"]
    result["budget"] = budget.state
    write(directory / "qualification.json", result)
    print(
        json.dumps(
            dict(stop_reason=reason, counts=result["counts"], budget=budget.state)
        ),
        flush=True,
    )
    return 0 if result["accepted"] else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        choices=[
            "freeze",
            "run",
            "freeze-a2",
            "run-a2",
            "freeze-a3",
            "run-a3",
            "repair-a3",
        ],
    )
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--original", type=Path, default=REPO / "build/dal-542-formal")
    parser.add_argument("--foreground-seconds", type=int, default=1800)
    parser.add_argument(
        "--binaries", type=Path, default=REPO / "build/dal-542-reference"
    )
    args = parser.parse_args()
    directory = args.directory.resolve()
    if args.action == "repair-a3":
        from a3_repair import repair

        repair(directory, args.original.resolve(), args.binaries.resolve())
        return 0
    if args.action in ("freeze-a3", "run-a3"):
        from a3_run import freeze_a3, run_a3

        if args.action == "freeze-a3":
            freeze_a3(directory, args.original.resolve(), args.binaries.resolve())
            return 0
        return run_a3(directory, args.foreground_seconds)
    if args.action in ("freeze-a2", "run-a2"):
        from a2_run import freeze_a2, run_a2

        if args.action == "freeze-a2":
            freeze_a2(directory, args.original.resolve(), args.binaries.resolve())
            return 0
        return run_a2(directory, args.foreground_seconds)
    if args.action == "freeze":
        freeze(directory, args.original.resolve(), args.binaries.resolve())
        return 0
    return run(directory)


if __name__ == "__main__":
    raise SystemExit(main())

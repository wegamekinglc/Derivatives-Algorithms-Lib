import argparse
import concurrent.futures
import json
import pathlib
import subprocess
import time

from bbscandidate import (
    CRR_NODES,
    PRIOR_BS,
    PRIOR_NODES,
    budget,
    full_proof,
    import_restart,
    make_plan,
    verify_native,
)
from bbsplan import METHOD
from bbsverify import price
from qualify import digest, write_json


def validate_plan(plan, executable):
    execution = verify_native(executable, plan["execution"])
    fresh = make_plan(execution)
    import_restart(fresh)
    for field in ["jobs", "bindings", "reuse_receipts"]:
        if fresh[field] != plan[field]:
            raise RuntimeError("frozen candidate plan/reuse payload changed")
    budget(plan["reserved_nodes"], plan["reserved_bs"])


def committed(path):
    data = subprocess.check_output(["git", "show", "HEAD:" + str(path)])
    if data != path.read_bytes():
        raise RuntimeError("commit frozen plan/control before numerical execution")


def materialize(directory, plan, executable):
    if (directory / "execution.json").exists():
        raise RuntimeError("preserve completed/stopped candidate execution; no retry")
    now = time.time()
    execution = dict(
        started_epoch=now,
        deadline_epoch=now + 600,
        shards=[],
        native=str(executable.resolve()),
        stopped=False,
    )
    for i, keys in enumerate(plan["shards"]):
        path = directory / f"prices-{i}.jsonl"
        if path.exists() or pathlib.Path(str(path) + ".results.jsonl").exists():
            raise RuntimeError("preserve candidate batch")
        rows = [native_job(plan["jobs"][key], now + 600) for key in keys]
        path.write_text(
            "".join(json.dumps(row, allow_nan=False) + "\n" for row in rows)
        )
        execution["shards"].append(
            dict(path=str(path), input_sha256=digest(path), keys=keys, state="RESERVED")
        )
    write_json(directory / "execution.json", execution)
    return execution


def native_job(row, deadline):
    fields = ["id", "method", "expected_failure", "charged_nodes", "reserved_bs"]
    return dict(
        {k: row[k] for k in fields},
        request=dict(row["request"], deadline_epoch=deadline),
    )


def run_shard(executable, shard, deadline):
    remaining = deadline - time.time()
    if remaining <= 0:
        return dict(state="NOT_STARTED", error="numerical deadline exhausted")
    path = pathlib.Path(shard["path"])
    try:
        with path.with_suffix(".log").open("w") as log:
            result = subprocess.run(
                [
                    "prlimit",
                    "--as=134217728",
                    "--",
                    str(executable.resolve()),
                    "--batchfile",
                    str(path),
                ],
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=remaining,
                check=False,
            )
        return dict(
            state="EXECUTED", returncode=result.returncode, completed_epoch=time.time()
        )
    except subprocess.TimeoutExpired:
        return dict(
            state="EXECUTED", returncode=-1, error="numerical timeout; child reaped"
        )


def execute(directory, executable, execution):
    pending = iter(execution["shards"])
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as workers:
        running = {}
        for _ in range(4):
            launch(
                workers, running, next(pending), executable, execution["deadline_epoch"]
            )
        while running:
            done, _ = concurrent.futures.wait(
                running, return_when=concurrent.futures.FIRST_COMPLETED
            )
            for future in done:
                shard = running.pop(future)
                shard.update(future.result())
                if shard.get("returncode") != 0:
                    execution["stopped"] = True
            if not execution["stopped"]:
                refill(
                    workers, running, pending, executable, execution["deadline_epoch"]
                )
            write_json(directory / "execution.json", execution)


def launch(workers, running, shard, executable, deadline):
    shard["state"] = "STARTED"
    running[workers.submit(run_shard, executable, shard, deadline)] = shard


def refill(workers, running, pending, executable, deadline):
    while len(running) < 4:
        shard = next(pending, None)
        if shard is None:
            return
        launch(workers, running, shard, executable, deadline)


def validate_output(row, planned, execution):
    if row["id"] != planned["id"] or row["method"] != METHOD:
        raise RuntimeError("candidate output identity differs")
    price(row["result"])
    values = row["result"]
    counts = [values["visited_nodes"], values["bs_evaluations"]]
    validate_counts(counts)
    if counts[0] > planned["charged_nodes"] or counts[1] > planned["reserved_bs"]:
        raise RuntimeError("candidate operations exceed reservation")
    if (
        row["max_rss_kib"] > 131072
        or row["finished_epoch"] > execution["deadline_epoch"]
    ):
        raise RuntimeError("candidate RSS/time limit exceeded")


def validate_counts(counts):
    if any(not isinstance(v, int) or v < 0 for v in counts):
        raise RuntimeError("invalid operation counter")


def collect(directory, plan, execution):
    rows = {
        key: job["reuse"]["row"] for key, job in plan["jobs"].items() if "reuse" in job
    }
    errors = []
    for shard in execution["shards"]:
        path = pathlib.Path(shard["path"])
        if digest(path) != shard["input_sha256"]:
            errors.append("input payload changed: " + str(path))
        output = pathlib.Path(str(path) + ".results.jsonl")
        if output.exists():
            collect_file(output, plan, execution, rows, errors)
    write_json(directory / "all-raw.json", rows)
    return rows, errors


def collect_file(path, plan, execution, rows, errors):
    for line in path.read_text().splitlines():
        try:
            row = json.loads(line)
            key = row["id"]
            if key in rows:
                raise RuntimeError("duplicate native result")
            validate_output(row, plan["jobs"][key], execution)
            rows[key] = row
        except (RuntimeError, ValueError, KeyError) as error:
            errors.append(str(error))


def resource_account(directory, plan, execution, rows):
    new = {k: r for k, r in rows.items() if "reuse" not in plan["jobs"][k]}
    attempted = attempted_keys(execution)
    charged = planned_sum(plan, attempted, "charged_nodes")
    bs = native_sum(new, "bs_evaluations")
    unknown = set(attempted) - set(new)
    bs_bound = bs + planned_sum(plan, unknown, "reserved_bs")
    budget(charged, bs_bound)
    return dict(
        prior_bbs_nodes=PRIOR_NODES,
        prior_bbs_bs=PRIOR_BS,
        old_crr_nodes=CRR_NODES,
        reserved_nodes=plan["reserved_nodes"],
        reserved_bs=plan["reserved_bs"],
        charged_nodes=charged,
        actual_known_nodes=native_sum(new, "visited_nodes"),
        actual_known_bs=bs,
        conservative_bs=bs_bound,
        unknown_attempts=len(unknown),
        scalar_calls=0,
        cumulative_bbs_nodes=PRIOR_NODES + charged,
        cumulative_bbs_bs=PRIOR_BS + bs_bound,
        combined_nodes=CRR_NODES + PRIOR_NODES + charged,
        completed_new=len(new),
        reused=84,
        numerical_seconds=time.time() - execution["started_epoch"],
        max_rss_kib=peak_rss(new),
        raw_bytes=raw_size(directory),
    )


def attempted_keys(execution):
    return [
        k for s in execution["shards"] if s["state"] != "RESERVED" for k in s["keys"]
    ]


def planned_sum(plan, keys, field):
    return sum(plan["jobs"][k][field] for k in keys)


def native_sum(rows, field):
    return sum(r["result"][field] for r in rows.values())


def peak_rss(rows):
    return max([r["max_rss_kib"] for r in rows.values()] or [0])


def raw_size(directory):
    return sum(p.stat().st_size for p in directory.rglob("*") if p.is_file())


def finish(directory, plan, executable, execution):
    rows, errors = collect(directory, plan, execution)
    usage = resource_account(directory, plan, execution, rows)
    if len(rows) != 792 or execution["stopped"]:
        errors.append("candidate prices missing or native execution stopped")
    if usage["numerical_seconds"] >= 600 or usage["raw_bytes"] > 67108864:
        errors.append("candidate total numerical window/raw limit exceeded")
    verify_native(executable, plan["execution"])
    proofs, status = candidate_proofs(plan, rows, errors)
    result = dict(
        status=status,
        errors=errors,
        proofs=proofs,
        resources=usage,
        mandatory_tree_replaced=False,
        qualification_promoted=False,
        input_hashes={s["path"]: s["input_sha256"] for s in execution["shards"]},
    )
    write_json(directory / "result.json", result)
    print(status, "bindings", {k[:10]: p["status"] for k, p in proofs.items()}, usage)


def candidate_proofs(plan, rows, errors):
    proofs = (
        {}
        if errors
        else {
            k: full_proof(plan, b["request"], rows) for k, b in plan["bindings"].items()
        }
    )
    status = (
        "PASS"
        if proofs and all(p["status"] == "PASS" for p in proofs.values())
        else "INCOMPLETE"
    )
    return proofs, status


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--driver", required=True, type=pathlib.Path)
    args = parser.parse_args()
    committed(args.plan)
    committed(pathlib.Path(".codex/artifacts/specs/dal-497-pde-risk-acceptance.md"))
    plan = json.loads(args.plan.read_text())
    if (
        digest(pathlib.Path(".codex/artifacts/specs/dal-497-pde-risk-acceptance.md"))
        != plan["contract_sha256"]
    ):
        raise RuntimeError("frozen numerical control changed")
    validate_plan(plan, args.driver)
    args.output.mkdir(parents=True, exist_ok=True)
    execution = materialize(args.output, plan, args.driver)
    execute(args.output, args.driver, execution)
    finish(args.output, plan, args.driver, execution)


if __name__ == "__main__":
    main()

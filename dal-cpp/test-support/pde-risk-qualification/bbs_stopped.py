import argparse
import json
import pathlib

from bbsverify import LIMITS, strict
from qualify import differences, write_json


def lookup(ledger, name, n, offset):
    for key, job in ledger["jobs"].items():
        s = job["spec"]
        if (
            s["kind"] == "pilot"
            and s["name"] == name
            and s["request"]["tree_steps"] == n
            and s["offset"] == offset
        ):
            return key, job
    raise RuntimeError("unplanned pilot request")


def member(ledger, name, n, a):
    rows = [lookup(ledger, name, n, o) for o in [0.0, -a, a]]
    missing = [
        key
        for key, j in rows
        if j["state"] != "COMPLETE" or "price" not in j["output"]["result"]
    ]
    if missing:
        return dict(vector=None, missing_or_failed=missing)
    base, low, high = [j["output"]["result"]["price"] for _, j in rows]
    return dict(
        vector=[
            base / 100,
            (high - low) / (2 * a),
            100 * (high - 2 * base + low) / a**2,
        ],
        missing_or_failed=[],
    )


def level(ledger, name, n):
    scales = []
    for a in [1.0, 0.5, 0.25]:
        raw = [member(ledger, name, n + p, a) for p in [0, 1]]
        complete = all(r["vector"] is not None for r in raw)
        mean = (
            [sum(r["vector"][i] for r in raw) / 2 for i in range(3)]
            if complete
            else None
        )
        scales.append(dict(spot_bump=a, raw=raw, mean=mean))
    return dict(n=n, scales=scales)


def delta(a, b):
    return differences(a, b) if a is not None and b is not None else None


def market_proof(ledger, name):
    levels = [level(ledger, name, n) for n in [4096, 8192, 16384]]
    final, previous = levels[-1], levels[-2]
    bump = delta(final["scales"][1]["mean"], final["scales"][2]["mean"])
    proofs = []
    for i, last in enumerate(final["scales"]):
        mesh = delta(last["mean"], previous["scales"][i]["mean"])
        parity = delta(last["raw"][0]["vector"], last["raw"][1]["vector"])
        values = [mesh, parity, bump]
        good = all(v is not None and strict(v) for v in values)
        proofs.append(
            dict(
                spot_bump=last["spot_bump"],
                status="PASS" if good else "INCOMPLETE",
                mesh_difference=mesh,
                parity_difference=parity,
                fine_extra_half_difference=bump,
                strict_limits=LIMITS,
            )
        )
    return dict(
        status="INCOMPLETE",
        reason="pilot stopped with native failures and missing rows",
        levels=levels,
        scales=proofs,
    )


def cached_american(ledger, witnesses):
    european = {
        row["spec"]["name"]: row["expected"]
        for row in witnesses["results"].values()
        if row["spec"]["kind"] == "european"
    }
    rows = {}
    for key, job in ledger["jobs"].items():
        s = job["spec"]
        if priced_pilot(job):
            r = job["output"]["result"]
            rows[key] = dict(
                continuation=r["root_continuation"] > r["root_intrinsic"]
                and not r["root_exercised"],
                obstacle_hits=r["obstacle_hits"],
                premium=(
                    r["price"] - european[s["name"]] if s["offset"] == 0.0 else None
                ),
            )
    return rows


def priced_pilot(job):
    return (
        job["spec"]["kind"] == "pilot"
        and job["state"] == "COMPLETE"
        and "price" in job["output"]["result"]
    )


def usage(ledger, completed):
    attempted = sum(j["charged_nodes"] for j in completed)
    actual_nodes = sum(j["output"]["result"]["visited_nodes"] for j in completed)
    actual_bs = (
        sum(j["output"]["result"]["bs_evaluations"] for j in completed)
        + ledger["scalar_calls"]
    )
    return dict(
        planned_jobs=len(ledger["jobs"]),
        executed_jobs=len(completed),
        unexecuted_jobs=len(ledger["jobs"]) - len(completed),
        reserved_nodes=ledger["charged_nodes"],
        attempted_charged_nodes=attempted,
        actual_visited_nodes=actual_nodes,
        actual_bs_calls=actual_bs,
        scalar_bs_calls=ledger["scalar_calls"],
        reserved_bs=ledger["reserved_bs"],
        old_consumed_nodes=ledger["old_consumed_nodes"],
        total_attempted_charged_nodes=ledger["old_consumed_nodes"] + attempted,
        total_reserved_nodes=ledger["old_consumed_nodes"] + ledger["charged_nodes"],
        total_cap=3003881833992,
        numeric_wall_seconds=max(j["output"]["finished_epoch"] for j in completed)
        - ledger["started_epoch"],
        max_rss_kib=max(j["output"]["max_rss_kib"] for j in completed),
    )


def failed_pilot(ledger):
    return {
        key: dict(spec=j["spec"], result=j["output"]["result"])
        for key, j in ledger["jobs"].items()
        if j["state"] == "COMPLETE"
        and j["spec"]["kind"] == "pilot"
        and "error" in j["output"]["result"]
    }


def report(directory):
    ledger = json.loads((directory / "ledger.json").read_text())
    completed = [j for j in ledger["jobs"].values() if j["state"] == "COMPLETE"]
    witness = json.loads((directory / "witnesses.json").read_text())
    budget = usage(ledger, completed)
    budget["raw_bytes"] = sum(
        p.stat().st_size for p in directory.rglob("*") if p.is_file()
    )
    result = dict(
        status="INCOMPLETE",
        mandatory_tree_replaced=False,
        witness_status=witness["status"],
        proofs={name: market_proof(ledger, name) for name in ["PUT", "CALL"]},
        unexpected_failures=failed_pilot(ledger),
        american=cached_american(ledger, witness),
        new_scalar_calls=0,
        resources=budget,
    )
    write_json(directory / "partial-proof.json", result)
    print(json.dumps(result["resources"], indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    report(parser.parse_args().output)

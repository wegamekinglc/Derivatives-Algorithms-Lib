import argparse
import json
import math
import pathlib

from bbsplan import METHOD, cache_key, identity
from bbsverify import check_witness_receipt, price
from campaign import controls, engine, unique_controls
from qualify import digest, write_json
from referenceplan import (
    binding,
    estimates,
    fingerprint,
    mean_members,
    offsets,
    physical,
    price_spec,
    scales,
    self_qualification,
)

PAIRS = [4096, 8192, 16384]
PRIOR_NODES = 8869730816
PRIOR_BS = 1511876
CRR_NODES = 2997881833992
NATIVE = "f6a72ce4dc17ac5bd62562ff33e73a21db4cac94aae0e36d5ff26a4bf5b8b518"
LIMITS = [2.5e-5, 5e-4, 5e-3, 7.5e-4, 7.5e-4, 7.5e-4]
RESTART = pathlib.Path(".codex/artifacts/validation/dal502-bbs-restart")
RAW_RESTART = pathlib.Path("build/dal502-bbs-restart/data")


def full_gate(values):
    return (
        values is not None
        and len(values) == 6
        and all(
            v is not None and math.isfinite(v) and 0 <= v < limit
            for v, limit in zip(values, LIMITS)
        )
    )


def budget(nodes, bs):
    checks = [
        nodes <= 42000000000,
        bs <= 7000000,
        PRIOR_NODES + nodes <= 51000000000,
        PRIOR_BS + bs <= 9000000,
        CRR_NODES + PRIOR_NODES + nodes <= 3048881833992,
    ]
    if nodes < 0 or bs < 0 or not all(checks):
        raise RuntimeError("candidate round/cumulative budget exceeded")


def native_request(request, n):
    r = physical(request)
    if not r["american"] or r["exercise"] or len(r["carry"]) != 1:
        raise RuntimeError(
            "candidate requires the frozen constant-carry American market"
        )
    return dict(
        spot=r["spot"],
        strike=100.0,
        vol=r["vol"],
        days=r["days"],
        call=r["call"],
        american=True,
        carry=r["carry"],
        tree_steps=n,
    )


def price_id(request, execution):
    return fingerprint(dict(method=METHOD, request=request, execution=execution))


def job(request, execution):
    n = request["tree_steps"]
    return dict(
        id=price_id(request, execution),
        method=METHOD,
        request=request,
        expected_failure=False,
        charged_nodes=(n + 1) * (n + 2) // 2,
        reserved_bs=n,
        uses=[],
    )


def add_binding_jobs(jobs, request, execution):
    for n in [n + p for n in PAIRS for p in [0, 1]]:
        for axis, offset in offsets(request):
            raw = price_spec(request, axis, offset, "tree", n)["request"]
            r = native_request(raw, n)
            key = price_id(r, execution)
            row = jobs.setdefault(key, job(r, execution))
            row["uses"].append(dict(binding=binding(request), axis=axis, offset=offset))


def make_plan(execution):
    bindings = {}
    jobs = {}
    for key, request in unique_controls("tree").items():
        names = [
            name
            for name, r in controls().items()
            if engine(name) == "tree" and binding(r) == key
        ]
        bindings[key] = dict(
            request=physical(request), actual_scales=scales(request), fixtures=names
        )
        add_binding_jobs(jobs, request, execution)
    return dict(method=METHOD, execution=execution, bindings=bindings, jobs=jobs)


def reuse_value(key, job, row, sha, request, execution):
    spec = job["spec"]
    checks = [
        job["state"] == "COMPLETE",
        spec["method"] == METHOD,
        spec["kind"] == "pilot",
        spec["request"] == request,
        key == cache_key(spec, execution),
        row["id"] == key,
        row["method"] == METHOD,
        row == job["output"],
        fingerprint(row) == sha,
        request["spot"] == 100.0 + spec["offset"],
    ]
    if not all(checks):
        raise RuntimeError("reused producer/input/offset/payload identity differs")
    return price(row["result"])


def restart_rows():
    accepted = json.loads((RESTART / "identity.json").read_text())
    rows = {}
    for name, record in accepted["raw_files"].items():
        path = pathlib.Path(name)
        if digest(path) != record["sha256"]:
            raise RuntimeError("restart raw payload changed")
        for line in path.read_text().splitlines():
            row = json.loads(line)
            if row["id"] in rows:
                raise RuntimeError("duplicate restart raw identity")
            rows[row["id"]] = dict(row=row, source=name, source_sha256=digest(path))
    return rows


def import_restart(plan):
    ledger = json.loads((RESTART / "ledger.json").read_text())
    if plan["execution"] != ledger["identity"] or ledger["bridges"]:
        raise RuntimeError("candidate producer differs from approved restart")
    if digest(RESTART / "ledger.json") != digest(RAW_RESTART / "ledger.json"):
        raise RuntimeError("restart ledger changed")
    check_witness_receipt(RAW_RESTART, ledger)
    rows = restart_rows()
    for key, old in ledger["jobs"].items():
        if old["spec"]["kind"] == "pilot":
            import_price(plan, key, old, rows[key])
    plan["reuse_receipts"] = dict(
        ledger=digest(RESTART / "ledger.json"),
        witnesses=digest(RAW_RESTART / "witnesses.json"),
        resources=digest(RESTART / "resource-account.json"),
    )


def import_price(plan, key, old, receipt):
    request = old["spec"]["request"]
    new = plan["jobs"][price_id(request, plan["execution"])]
    row = receipt["row"]
    sha = fingerprint(row)
    reuse_value(key, old, row, sha, new["request"], plan["execution"])
    if not any(
        u["axis"] in ["base", "spot"] and u["offset"] == old["spec"]["offset"]
        for u in new["uses"]
    ):
        raise RuntimeError("reused offset has no matching binding use")
    new["reuse"] = dict(receipt, original_id=key, payload_sha256=sha)


def shard_jobs(jobs):
    shards = [[] for _ in range(10)]
    loads = [0] * len(shards)
    pending = [j for j in jobs.values() if "reuse" not in j]
    for row in sorted(pending, key=lambda j: j["charged_nodes"], reverse=True):
        index = loads.index(min(loads))
        shards[index].append(row)
        loads[index] += row["charged_nodes"]
    for shard in shards:
        nodes, bs = reservations(shard)
        if nodes > 6000000000 or bs > 2000000:
            raise RuntimeError("native per-batch reservation exceeded")
    return shards


def reservations(jobs):
    return sum(j["charged_nodes"] for j in jobs), sum(j["reserved_bs"] for j in jobs)


def shard_ids(jobs):
    return [[j["id"] for j in shard] for shard in shard_jobs(jobs)]


def verify_native(executable, expected=None):
    value = identity(executable)
    if value["executable"] != NATIVE or (expected is not None and value != expected):
        raise RuntimeError("frozen native/source/configuration identity changed")
    accepted = json.loads((RESTART / "identity.json").read_text())
    if value != accepted["native"]:
        raise RuntimeError("delivered native source/configuration differs")
    return value


def freeze(path, executable):
    if path.exists():
        raise RuntimeError("preserve frozen candidate plan")
    plan = make_plan(verify_native(executable))
    import_restart(plan)
    new = [j for j in plan["jobs"].values() if "reuse" not in j]
    nodes, bs = reservations(new)
    if (len(plan["jobs"]), len(new), nodes, bs) != (792, 708, 41587475848, 6766946):
        raise RuntimeError("approved exact union/reservation differs")
    budget(nodes, bs)
    plan.update(
        reserved_nodes=nodes,
        reserved_bs=bs,
        scalar_reservation=0,
        binding_count=12,
        new_prices=708,
        reused_prices=84,
        decision="01a1b95b-ac6f-7cd2-8896-f5662f183e8b",
        contract_sha256=digest(
            pathlib.Path(".codex/artifacts/specs/dal-497-pde-risk-acceptance.md")
        ),
        shards=shard_ids(plan["jobs"]),
    )
    write_json(path, plan)
    print("frozen 12 bindings / 792 prices / 84 reused / 708 new", nodes, bs)


def full_proof(plan, request, results):
    levels = []
    for n in PAIRS:
        members = []
        for p in [0, 1]:

            def read(axis, offset):
                raw = price_spec(request, axis, offset, "tree", n + p)["request"]
                r = native_request(raw, n + p)
                return price(results[price_id(r, plan["execution"])]["result"])

            members.append(estimates(request, read))
        levels.append(dict(n=n, members=members, mean=mean_members(members)))
    result = qualify_levels(levels)
    result.update(actual_scales=scales(request), strict_limits=LIMITS)
    return result


def binding_request(record):
    actual = record["actual_scales"]
    request = dict(
        record["request"],
        h=actual[1][0],
        vol_bump=actual[0][1],
        carry_bump=actual[0][2],
    )
    if scales(request) != actual:
        raise RuntimeError("candidate binding scales cannot be restored")
    return request


def qualify_levels(levels):
    result = self_qualification(levels, "tree")
    gates = (
        result["scale_mesh_differences"]
        + result["parity_differences"]
        + [result["bump_difference"]]
    )
    result["status"] = "PASS" if all(full_gate(v) for v in gates) else "INCOMPLETE"
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", required=True, type=pathlib.Path)
    parser.add_argument("--driver", required=True, type=pathlib.Path)
    args = parser.parse_args()
    freeze(args.plan, args.driver)

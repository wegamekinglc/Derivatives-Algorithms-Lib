import argparse
import json
import math
import pathlib
import time

from bbsplan import METHOD, NODE_CAP, OLD_COST, collect, write_workers
from qualify import differences, write_json

LIMITS = [2.5e-5, 5e-4, 5e-3]


def price(row):
    value = row.get("price")
    if "error" in row or value is None or not math.isfinite(value):
        raise RuntimeError("BBS reference missing/failed/nonfinite")
    return value


def strict(values):
    return len(values) == 3 and all(
        v is not None and math.isfinite(v) and v < t for v, t in zip(values, LIMITS)
    )


def self_proof(levels):
    final, previous = levels[-1], levels[-2]
    mesh = [differences(final["mean"][s], previous["mean"][s]) for s in range(3)]
    parity = [
        differences(final["members"][0][s], final["members"][1][s]) for s in range(3)
    ]
    bump = differences(final["mean"][1], final["mean"][2])
    return dict(
        status=(
            "PASS" if all(strict(d) for d in mesh + parity + [bump]) else "INCOMPLETE"
        ),
        levels=levels,
        mesh_differences=mesh,
        parity_differences=parity,
        fine_extra_half_difference=bump,
        strict_limits=LIMITS,
    )


class Scalar:
    def __init__(self, ledger):
        self.ledger = ledger

    def bs(self, r, spot, years):
        if self.ledger["scalar_calls"] >= self.ledger["scalar_reservation"]:
            raise RuntimeError("scalar BS reservation exhausted")
        self.ledger["scalar_calls"] += 1
        rate, yield_ = r["carry"][0][1:]
        sd = r["vol"] * math.sqrt(years)
        d2 = (
            math.log(spot / r["strike"]) + (rate - yield_) * years - 0.5 * sd * sd
        ) / sd
        d1 = d2 + sd
        sign = 1 if r["call"] else -1
        cdf = lambda x: (1 + math.erf(x / math.sqrt(2))) / 2
        return sign * (
            spot * math.exp(-yield_ * years) * cdf(sign * d1)
            - r["strike"] * math.exp(-rate * years) * cdf(sign * d2)
        )


def intrinsic(r, spot):
    return max(spot - r["strike"] if r["call"] else r["strike"] - spot, 0.0)


def formula(r, scalar):
    n = r["tree_steps"]
    dt = r["days"] / 365 / n
    if n == 1:
        euro = scalar.bs(r, r["spot"], dt)
        return max(intrinsic(r, r["spot"]), euro) if r["american"] else euro
    u = math.exp(r["vol"] * math.sqrt(dt))
    d = 1 / u
    rate, yield_ = r["carry"][0][1:]
    p = (math.exp((rate - yield_) * dt) - d) / (u - d)
    down = max(intrinsic(r, r["spot"] * d), scalar.bs(r, r["spot"] * d, dt))
    up = max(intrinsic(r, r["spot"] * u), scalar.bs(r, r["spot"] * u, dt))
    return max(
        intrinsic(r, r["spot"]), math.exp(-rate * dt) * ((1 - p) * down + p * up)
    )


def witness_row(job, scalar):
    spec = job["spec"]
    raw = job["output"]["result"]
    r = spec["request"]
    if spec["kind"] == "failure":
        return failure_witness(spec, raw)
    observed = price(raw)
    expected = (
        scalar.bs(r, r["spot"], r["days"] / 365)
        if spec["kind"] == "european"
        else formula(r, scalar)
    )
    limit = 2.5e-5 if spec["kind"] == "european" else 1e-12
    error = abs(observed - expected) / r["strike"]
    return dict(
        status="PASS" if error < limit else "FAIL",
        normalized_error=error,
        limit=limit,
        expected=expected,
        raw=raw,
    )


def failure_witness(spec, raw):
    messages = [
        "step count",
        "step count",
        "nonpositive",
        "nonpositive",
        "requires constant",
        "invalid probability",
        "wall-clock",
        "nonpositive",
    ]
    index = int(spec["name"].split("-")[-1])
    good = "price" not in raw and messages[index] in raw.get("error", "")
    if index == 7:
        good = good and raw["visited_nodes"] == 2 and raw["bs_evaluations"] == 1
    return dict(
        status="PASS" if good else "FAIL", raw=raw, expected_message=messages[index]
    )


def witnesses(directory, ledger):
    scalar = Scalar(ledger)
    results = {}
    for key, job in ledger["jobs"].items():
        if job["spec"]["kind"] != "pilot":
            if job["state"] != "COMPLETE":
                raise RuntimeError("missing BBS witness")
            results[key] = dict(witness_row(job, scalar), spec=job["spec"])
    status = "PASS" if all(r["status"] == "PASS" for r in results.values()) else "FAIL"
    write_json(directory / "witnesses.json", dict(status=status, results=results))
    write_json(directory / "ledger.json", ledger)
    if status != "PASS":
        raise RuntimeError("BBS witnesses failed; stop pilot")
    return results


def vectors(prices):
    base = prices[0.0]
    return [
        [
            base / 100,
            (prices[a] - prices[-a]) / (2 * a),
            100 * (prices[a] - 2 * base + prices[-a]) / a**2,
        ]
        for a in [1.0, 0.5, 0.25]
    ]


def pilot_proofs(ledger):
    output = {}
    for name in ["PUT", "CALL"]:
        levels = []
        for n in [4096, 8192, 16384]:
            members = [
                vectors(pilot_prices(ledger, name, n + parity)) for parity in [0, 1]
            ]
            mean = [
                [sum(m[s][i] for m in members) / 2 for i in range(3)] for s in range(3)
            ]
            levels.append(dict(n=n, members=members, mean=mean))
        output[name] = self_proof(levels)
    return output


def pilot_prices(ledger, name, n):
    result = {}
    for job in ledger["jobs"].values():
        spec = job["spec"]
        if (
            spec["kind"] == "pilot"
            and spec["name"] == name
            and spec["request"]["tree_steps"] == n
        ):
            if job["state"] != "COMPLETE":
                raise RuntimeError("BBS pilot row missing")
            result[spec["offset"]] = price(job["output"]["result"])
    if set(result) != {0.0, -1.0, 1.0, -0.5, 0.5, -0.25, 0.25}:
        raise RuntimeError("BBS spot stencil incomplete")
    return result


def american_checks(ledger):
    results = {}
    scalar = Scalar(ledger)
    for key, job in ledger["jobs"].items():
        spec = job["spec"]
        if spec["kind"] == "pilot" and spec["offset"] == 0:
            raw = job["output"]["result"]
            r = spec["request"]
            european = scalar.bs(r, r["spot"], r["days"] / 365)
            good = (
                price(raw) > european
                and raw["root_continuation"] > raw["root_intrinsic"]
                and not raw["root_exercised"]
                and raw["obstacle_hits"] > 0
            )
            results[key] = dict(
                status="PASS" if good else "FAIL",
                premium=price(raw) - european,
                raw=raw,
                european=european,
            )
    return results


def resources(directory, ledger):
    jobs = list(ledger["jobs"].values())
    actual_nodes = sum(j["output"]["result"]["visited_nodes"] for j in jobs)
    bs = (
        sum(j["output"]["result"]["bs_evaluations"] for j in jobs)
        + ledger["scalar_calls"]
    )
    elapsed = time.time() - ledger["started_epoch"]
    raw_bytes = sum(p.stat().st_size for p in directory.rglob("*") if p.is_file())
    return dict(
        charged_nodes=ledger["charged_nodes"],
        actual_nodes=actual_nodes,
        bs_calls=bs,
        reserved_bs=ledger["reserved_bs"],
        scalar_calls=ledger["scalar_calls"],
        elapsed_seconds=elapsed,
        raw_bytes=raw_bytes,
        max_rss_kib=max(j["output"]["max_rss_kib"] for j in jobs),
        old_consumed_nodes=OLD_COST,
        total_charged_nodes=OLD_COST + ledger["charged_nodes"],
        total_node_cap=OLD_COST + NODE_CAP,
    )


def finish(directory, ledger):
    proofs = pilot_proofs(ledger)
    american = american_checks(ledger)
    status = (
        "PASS" if all(p["status"] == "PASS" for p in proofs.values()) else "INCOMPLETE"
    )
    if any(r["status"] == "FAIL" for r in american.values()):
        status = "FAIL"
    usage = resources(directory, ledger)
    if usage["elapsed_seconds"] > 600 or usage["raw_bytes"] > 67108864:
        status = "INCOMPLETE"
    result = dict(
        status=status,
        pilot_only=True,
        mandatory_tree_replaced=False,
        proofs=proofs,
        american=american,
        resources=usage,
    )
    write_json(directory / "pilot-result.json", result)
    write_json(directory / "ledger.json", ledger)
    print(status, json.dumps(result["resources"]))


def execute(directory, phase):
    if (directory / "pilot-result.json").exists():
        raise RuntimeError("preserve completed/stopped BBS result")
    ledger = json.loads((directory / "ledger.json").read_text())
    try:
        collect(directory, ledger)
        if phase == "witness":
            witnesses(directory, ledger)
            write_workers(directory, ledger, "pilot")
        else:
            finish(directory, ledger)
    except RuntimeError as error:
        ledger["stop_reason"] = str(error)
        write_json(directory / "ledger.json", ledger)
        status = "INCOMPLETE"
        witness = directory / "witnesses.json"
        if witness.exists() and json.loads(witness.read_text())["status"] == "FAIL":
            status = "FAIL"
        write_json(
            directory / "pilot-result.json",
            dict(status=status, reason=str(error), pilot_only=True),
        )
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("phase", choices=["witness", "finish"])
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    execute(args.output, args.phase)

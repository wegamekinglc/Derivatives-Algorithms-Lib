import argparse
import copy
import json
import pathlib
import time

from qualify import digest, write_json
from referenceplan import fingerprint

METHOD = "BBS-last-step-v1"
NODE_CAP = 6000000000
BS_CAP = 2000000
OLD_COST = 2997881833992
LEVELS = [4096, 4097, 8192, 8193, 16384, 16385]


def request(call, spot, n, american=True):
    return dict(
        spot=spot,
        strike=100.0,
        vol=0.2,
        days=365.0,
        call=call,
        american=american,
        carry=[[365.0, 0.05, 0.08 if call else 0.0]],
        tree_steps=n,
    )


def spec(kind, name, value, offset=0.0):
    return dict(method=METHOD, kind=kind, name=name, offset=offset, request=value)


def formula_specs():
    result = []
    for call, spot, american in [
        (False, 100.0, True),
        (False, 50.0, True),
        (True, 150.0, True),
        (False, 50.0, False),
        (True, 150.0, False),
    ]:
        result.append(spec("formula", "one", request(call, spot, 1, american)))
    for call, spot in [(False, 50.0), (True, 150.0), (False, 100.0), (True, 100.0)]:
        result.append(spec("formula", "two", request(call, spot, 2)))
    return result


def failure_specs():
    changes = [
        dict(tree_steps=0),
        dict(tree_steps=16386),
        dict(spot=0.0),
        dict(vol=0.0),
        dict(carry=[[137.0, 0.05, 0.0], [365.0, 0.05, 0.0]]),
        dict(vol=0.01, carry=[[365.0, 1.0, 0.0]]),
        dict(deadline_epoch=1.0),
        dict(tree_steps=2, spot=1.75e308, call=True, carry=[[365.0, 0.05, 0.08]]),
    ]
    return [
        spec("failure", f"failure-{i}", dict(request(False, 100.0, 1), **change))
        for i, change in enumerate(changes)
    ]


def specifications():
    result = []
    for call in [False, True]:
        name = "CALL" if call else "PUT"
        for n in LEVELS:
            for offset in [0.0, -1.0, 1.0, -0.5, 0.5, -0.25, 0.25]:
                result.append(
                    spec("pilot", name, request(call, 100.0 + offset, n), offset)
                )
            result.append(spec("european", name, request(call, 100.0, n, False)))
    return result + formula_specs() + failure_specs()


def charges(specification):
    n = specification["request"]["tree_steps"]
    return ((n + 1) * (n + 2) // 2, n) if 1 <= n <= 16385 else (0, 0)


def reserve(ledger, specification):
    nodes, bs = charges(specification)
    if (
        ledger["charged_nodes"] + nodes > NODE_CAP
        or ledger["reserved_bs"] + bs > BS_CAP
    ):
        raise RuntimeError("BBS cumulative reservation exhausted")
    ledger["charged_nodes"] += nodes
    ledger["reserved_bs"] += bs


def cache_key(specification, identity):
    return fingerprint(dict(spec=specification, execution=identity))


def identity(executable):
    source = pathlib.Path(__file__).resolve().parent
    return dict(
        method=METHOD,
        executable=digest(executable),
        configuration=digest(executable.parent / "CMakeCache.txt"),
        compile_commands=digest(executable.parent / "compile_commands.json"),
        sources={
            name: digest(source / name)
            for name in ["bbs.hpp", "bbs_driver.cpp", "reference.hpp"]
        },
    )


def prepare(directory, executable):
    directory.mkdir(parents=True, exist_ok=True)
    if (directory / "ledger.json").exists():
        raise RuntimeError("preserve prior BBS ledger; it cannot be reset")
    execution = identity(executable)
    now = time.time()
    ledger = dict(
        method=METHOD,
        identity=execution,
        old_consumed_nodes=OLD_COST,
        charged_nodes=0,
        reserved_bs=100,
        scalar_reservation=100,
        scalar_calls=0,
        started_epoch=now,
        deadline_epoch=now + 600,
        cache={},
        bridges={},
        jobs={},
    )
    for specification in specifications():
        reserve(ledger, specification)
        key = cache_key(specification, execution)
        nodes, bs = charges(specification)
        value = copy.deepcopy(specification["request"])
        value.setdefault("deadline_epoch", ledger["deadline_epoch"])
        ledger["jobs"][key] = dict(
            spec=specification,
            state="RESERVED",
            charged_nodes=nodes,
            reserved_bs=bs,
            job=dict(
                id=key,
                method=METHOD,
                expected_failure=specification["kind"] == "failure",
                request=value,
                charged_nodes=nodes,
                reserved_bs=bs,
            ),
        )
    write_json(directory / "ledger.json", ledger)
    write_workers(directory, ledger, "witness")
    print(
        "reserved",
        ledger["charged_nodes"],
        "nodes",
        ledger["reserved_bs"],
        "BS calls including scalar witnesses",
    )


def write_workers(directory, ledger, phase):
    if time.time() >= ledger["deadline_epoch"]:
        raise RuntimeError("BBS numerical deadline exhausted")
    jobs = [
        r["job"]
        for r in ledger["jobs"].values()
        if (r["spec"]["kind"] == "pilot") == (phase == "pilot")
    ]
    paths = []
    for i in range(4):
        path = directory / f"{phase}-{i}.jsonl"
        if path.exists():
            raise RuntimeError("preserve recorded BBS phase")
        path.write_text(
            "".join(json.dumps(row, allow_nan=False) + "\n" for row in jobs[i::4])
        )
        paths.append(str(path.resolve()))
    (directory / (phase + ".workers.txt")).write_text("\n".join(paths) + "\n")


def collect(directory, ledger):
    if ledger["bridges"] or ledger["method"] != METHOD:
        raise RuntimeError("BBS cannot import CRR identities or bridges")
    seen = set()
    for path in directory.glob("*.results.jsonl"):
        for line in path.read_text().splitlines():
            row = json.loads(line)
            key = row["id"]
            if key in seen or row["method"] != METHOD:
                raise RuntimeError("duplicate/mismatched BBS identity")
            seen.add(key)
            job = ledger["jobs"][key]
            check_resource_result(row, job, ledger)
            job.update(state="COMPLETE", output=row)
            ledger["cache"][key] = row["result"]
    write_json(directory / "ledger.json", ledger)
    return ledger


def check_resource_result(row, job, ledger):
    r = row["result"]
    if (
        r["visited_nodes"] > job["charged_nodes"]
        or r["bs_evaluations"] > job["reserved_bs"]
    ):
        raise RuntimeError("BBS actual operations exceed reservation")
    if row["max_rss_kib"] > 131072 or row["finished_epoch"] > ledger["deadline_epoch"]:
        raise RuntimeError("BBS RSS/time limit exceeded")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--driver", type=pathlib.Path, required=True)
    args = parser.parse_args()
    prepare(args.output, args.driver)

import argparse
import json
import pathlib
import time

from qualify import digest, write_json
from referenceplan import node_charge, price_key

NODE_CAP = 3000000000000
DISK_CAP = 67108864


class BudgetExceeded(RuntimeError):
    pass


def identity(executable):
    root = pathlib.Path(__file__).resolve().parent
    return dict(
        executable=digest(executable),
        reference_source=digest(root / "reference.hpp"),
        driver_source=digest(root / "driver.cpp"),
        flags="GCC15.2 -O3 -DNDEBUG C++17 x86-64",
        configuration=digest(executable.parent / "CMakeCache.txt"),
        compile_commands=digest(executable.parent / "compile_commands.json"),
    )


def initialize(directory, executable):
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / "ledger.json"
    current = identity(executable)
    if path.exists():
        ledger = json.loads(path.read_text())
        if ledger["identity"] != current:
            raise RuntimeError("reference executable/source identity changed; prior ledger cannot be reset")
        return ledger
    ledger = dict(
        identity=current,
        started_epoch=time.time(),
        deadline_epoch=time.time() + 5400,
        charged_nodes=0,
        node_cap=NODE_CAP,
        workers=4,
        rss_kib_per_worker=131072,
        total_rss_kib=524288,
        disk_bytes=DISK_CAP,
        jobs={},
        bridges={},
        cache={},
    )
    write_json(path, ledger)
    return ledger


def disk_bytes(directory):
    return sum(p.stat().st_size for p in directory.rglob("*") if p.is_file())


def guard(directory, ledger):
    if time.time() >= ledger["deadline_epoch"]:
        raise BudgetExceeded("reference wall-clock budget exhausted")
    if disk_bytes(directory) >= DISK_CAP:
        raise BudgetExceeded("reference raw-output budget exhausted")


def enqueue(ledger, spec):
    key = price_key(spec, ledger["identity"])
    if key in ledger["jobs"] or key in ledger["bridges"]:
        return None
    nodes = node_charge(spec)
    if ledger["charged_nodes"] + nodes > NODE_CAP:
        raise BudgetExceeded("reference cumulative cache-miss node budget exhausted")
    ledger["charged_nodes"] += nodes
    request = dict(spec["request"])
    if spec["mode"] == "tree":
        request["deadline_epoch"] = ledger["deadline_epoch"]
    job = dict(id=key, mode=spec["mode"], request=request, nodes=nodes)
    ledger["jobs"][key] = dict(spec=spec, nodes=nodes, state="CHARGED")
    return job


def schedule_checked(directory, ledger, specs, phase):
    guard(directory, ledger)
    if (directory / f"{phase}-0.jsonl").exists():
        raise RuntimeError("phase already recorded; raw jobs/results must not be overwritten")
    jobs = []
    for spec in specs:
        job = enqueue(ledger, spec)
        if job is not None:
            jobs.append(job)
    files = []
    for worker in range(4):
        path = directory / f"{phase}-{worker}.jsonl"
        path.write_text("".join(json.dumps(j) + "\n" for j in jobs[worker::4]))
        files.append(str(path.resolve()))
    (directory / "workers.txt").write_text("\n".join(files) + "\n")
    write_json(directory / "ledger.json", ledger)
    print(phase, "misses", len(jobs), "cumulative charged nodes", ledger["charged_nodes"])


def schedule(directory, ledger, specs, phase):
    try:
        schedule_checked(directory, ledger, specs, phase)
    except BudgetExceeded as error:
        ledger["budget_stop"] = str(error)
        write_json(directory / "ledger.json", ledger)
        raise


def ingest_file(path, ledger):
    for line in path.read_text().splitlines():
        row = json.loads(line)
        key = row["id"]
        job = ledger["jobs"][key]
        if row["nodes"] != job["nodes"]:
            raise RuntimeError("native node charge differs from reserved budget")
        if row["max_rss_kib"] > ledger["rss_kib_per_worker"]:
            raise RuntimeError("reference worker RSS budget exceeded")
        job.update(
            state="COMPLETE",
            result=row["result"],
            elapsed_seconds=row["elapsed_seconds"],
            max_rss_kib=row["max_rss_kib"],
        )
        ledger["cache"][key] = row["result"]


def collect(directory):
    ledger = json.loads((directory / "ledger.json").read_text())
    if disk_bytes(directory) >= DISK_CAP:
        ledger["budget_stop"] = "reference raw-output budget exhausted"
    for path in directory.glob("*.results.jsonl"):
        ingest_file(path, ledger)
    write_json(directory / "ledger.json", ledger)
    return ledger


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=pathlib.Path)
    collect(parser.parse_args().directory)

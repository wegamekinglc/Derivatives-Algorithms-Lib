"""Durable cumulative A/1 budget and checked scalar reservations."""

import hashlib
import json
import os
import time
from pathlib import Path

LIMITS = dict(
    solves=376,
    pde_seconds=21600.0,
    mc0_seconds=21600.0,
    mc1_seconds=21600.0,
    total_seconds=64800.0,
    rss_bytes=6 * 2**30,
    evidence_bytes=2**30,
    paths_per_family=16 * 2**20,
)


def checked_address(family, batch, path, coordinate, dimension=8728):
    if dimension <= 0:
        raise ValueError("Invalid scalar dimension")
    if not (
        0 <= family < 2
        and 0 <= batch < 16
        and 0 <= path < 2**20
        and 0 <= coordinate < 8728
    ):
        raise ValueError("Invalid frozen coordinate")
    value = ((family * 16 + batch) * 2**20 + path) * dimension + coordinate
    if value >= 2**64 or 2 * 16 * 2**20 * dimension >= 2**64:
        raise OverflowError("Scalar reservation overflow")
    return value


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Budget:
    def __init__(self, path, identity, limits=None, create=False):
        self.path = Path(path)
        self.journal = self.path.with_suffix(".jsonl")
        self.limits = {**LIMITS, **(limits or {})}
        if self.path.exists():
            self.state = json.loads(self.path.read_text())
            if (
                self.state["identity"] != identity
                or self.state["limits"] != self.limits
            ):
                raise RuntimeError("Cumulative budget identity mismatch")
            last = json.loads(self.journal.read_text().splitlines()[-1])
            if last["state_sha256"] != digest(self.path):
                raise RuntimeError("Cumulative budget journal mismatch")
        else:
            if not create or self.journal.exists():
                raise RuntimeError("Missing cumulative budget; cannot reset")
            self.state = dict(
                identity=identity,
                limits=self.limits,
                solves_reserved=0,
                solves_completed=0,
                paths=[0, 0],
                paths_completed=[0, 0],
                seconds={},
                total_seconds=0.0,
                sequence=0,
                previous_hash=None,
                active_phase=None,
            )
            self.save("created")
        self.last_tick = time.monotonic()

    def save(self, event):
        prior = digest(self.path) if self.path.exists() else None
        self.state["previous_hash"] = prior
        self.state["sequence"] += 1
        self.state["last_event"] = event
        self.state["utc"] = time.time()
        temporary = self.path.with_suffix(".next")
        temporary.write_text(json.dumps(self.state, indent=2, allow_nan=False) + "\n")
        os.replace(temporary, self.path)
        with self.journal.open("a") as stream:
            stream.write(
                json.dumps(
                    dict(
                        sequence=self.state["sequence"],
                        previous_hash=prior,
                        state_sha256=digest(self.path),
                        event=event,
                        utc=self.state["utc"],
                        solves_reserved=self.state["solves_reserved"],
                        total_seconds=self.state["total_seconds"],
                    )
                )
                + "\n"
            )

    def begin(self, phase):
        if self.state["active_phase"]:
            raise RuntimeError(
                "Unclosed prior phase; recover cumulative budget before continuing"
            )
        self.last_tick = time.monotonic()
        self.state["active_phase"] = phase
        self.save("begin:" + phase)

    def end(self, phase):
        self.tick(phase)
        self.state["active_phase"] = None
        self.save("end:" + phase)

    def reserve_solve(self, name):
        if self.state["solves_reserved"] >= self.limits["solves"]:
            raise RuntimeError("INCOMPLETE: cumulative solve limit")
        self.state["solves_reserved"] += 1
        self.save("reserve:" + name)

    def completed(self):
        self.state["solves_completed"] += 1
        self.save("completed")

    def tick(self, phase):
        now = time.monotonic()
        elapsed = now - self.last_tick
        self.last_tick = now
        self.state["seconds"][phase] = self.state["seconds"].get(phase, 0.0) + elapsed
        self.state["total_seconds"] += elapsed
        self.save("tick:" + phase)
        if self.state["total_seconds"] >= self.limits["total_seconds"]:
            raise RuntimeError("INCOMPLETE: total time limit")
        if self.state["seconds"][phase] >= self.limits.get(
            phase + "_seconds", float("inf")
        ):
            raise RuntimeError("INCOMPLETE: stage time limit")

    def paths(self, family, count):
        if (
            count < self.state["paths"][family]
            or count > self.limits["paths_per_family"]
        ):
            raise RuntimeError("INCOMPLETE: cumulative path limit")
        self.state["paths"][family] = count
        self.save("paths")

    def resources(self, directory, pids=()):
        size = sum(p.stat().st_size for p in Path(directory).rglob("*") if p.is_file())
        if size > self.limits["evidence_bytes"]:
            raise RuntimeError("INCOMPLETE: evidence limit")
        for pid in (os.getpid(), *pids):
            status = Path(f"/proc/{pid}/status")
            if status.exists():
                rss = next(
                    (
                        int(s.split()[1]) * 1024
                        for s in status.read_text().splitlines()
                        if s.startswith("VmRSS:")
                    ),
                    0,
                )
                if rss >= self.limits["rss_bytes"]:
                    raise RuntimeError("INCOMPLETE: RSS limit")

    def completed_paths(self, family, count):
        if not 0 <= count <= self.state["paths"][family]:
            raise RuntimeError("Invalid completed path accounting")
        self.state["paths_completed"][family] = count
        self.save("completed-paths")

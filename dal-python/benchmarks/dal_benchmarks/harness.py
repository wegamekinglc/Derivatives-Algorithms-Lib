"""Timing boundary and serializable evidence, independent of DAL imports."""

from dataclasses import dataclass
import statistics
import time
from typing import Any, Callable


@dataclass
class Workload:
    run: Callable[[], Any]
    validate: Callable[[Any], None]


@dataclass
class Case:
    name: str
    cpp_target: str
    workload: dict
    prepare: Callable[[], Workload]

    def description(self):
        return {
            "name": self.name,
            "cpp_target": self.cpp_target,
            "workload": self.workload,
        }


def require(condition, message):
    # Keep validation active under python -O too.
    if not condition:
        raise ValueError(message)


def measure(case, *, samples=10, warmups=2, clock=time.perf_counter_ns):
    require(
        samples > 0 and warmups >= 0, "samples must be positive and warmups nonnegative"
    )
    work = case.prepare()
    work.validate(work.run())
    for _ in range(warmups):
        work.validate(work.run())
    durations = []
    for _ in range(samples):
        start = clock()
        value = work.run()
        elapsed = clock() - start
        work.validate(value)
        require(elapsed > 0, "nonpositive benchmark duration")
        durations.append(elapsed)
        # Destruction of the prior result must not leak into the next sample.
        del value
    return dict(
        case.description(),
        samples_ns=durations,
        min_ns=min(durations),
        median_ns=statistics.median(durations),
        max_ns=max(durations),
        status="passed",
    )


def select_cases(cases, groups, name_filter):
    unknown = set(groups) - {case.cpp_target for case in cases}
    require(not unknown, f"Unknown or unavailable benchmark groups: {sorted(unknown)}")
    selected = [
        case
        for case in cases
        if (not groups or case.cpp_target in groups) and name_filter in case.name
    ]
    require(bool(selected), "No benchmark cases selected")
    return selected

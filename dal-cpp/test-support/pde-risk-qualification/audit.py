#!/usr/bin/env python3
import argparse
import datetime
import json
import math
import pathlib
from functools import partial

from qualify import (
    differences,
    fixtures,
    load,
    normalized,
    scalar_price,
    within,
    write_json,
)


def optional_json(path):
    return json.loads(path.read_text()) if path.exists() else None


def scenario_evidence(s):
    return dict(
        scenario=s["scenarioId_"],
        status=s["status_"],
        iteration=s["maxIterationChange_"],
        linear=s["maxLinearBackwardError_"],
        obstacle=s["maxObstacleViolation_"],
        dual=s["maxDualViolation_"],
        complementarity=s["maxComplementarityResidual_"],
    )


def nullable_limit(value, limit):
    return value is None or value <= limit


def residual_conditions(s, epsilon, american):
    original = [
        nullable_limit(s[key], epsilon)
        for key in [
            "maxObstacleViolation_",
            "maxDualViolation_",
            "maxComplementarityResidual_",
        ]
    ]
    iteration = (
        not american
        or s["maxIterationChange_"] is not None
        and s["maxIterationChange_"] <= epsilon
    )
    return (
        [nullable_limit(s["maxLinearBackwardError_"], 1e-12)] + original + [iteration]
    )


def residuals(check, name, r, request):
    for s in r["scenarios"]:
        if s["status_"] == "SUCCESS":
            check(
                name + "-residual-" + s["scenarioId_"],
                residual_conditions(
                    s, r["plan"]["epsilon_"], request.get("american", False)
                ),
                scenario_evidence(s),
            )


def capacities(check, name, r):
    check(
        name + "-capacity",
        [
            r["execution"]["workUnits_"] <= r["original"]["risk_"]["maxWorkUnits_"],
            r["execution"]["numericPayloadBytes_"]
            <= r["plan"]["plannedNumericPayloadBytes_"],
            r["execution"]["peakWorkspaceBytes_"]
            <= r["plan"]["plannedWorkspaceBytes_"],
            r["execution"]["peakNativeBytes_"] <= r["plan"]["plannedPeakNativeBytes_"],
        ],
        r["execution"],
    )


def general(check, outputs, requests):
    for name, r in outputs.items():
        if "plan" in r:
            residuals(check, name, r, requests[name])
            capacities(check, name, r)


def step_conditions(p, knot, step):
    start = p["timeGrid_"][step["timeIndex_"]]["timeYears_"] * 365
    end = p["timeGrid_"][step["sourceTimeIndex_"]]["timeYears_"] * 365
    return [
        not start + 1e-10 < knot < end - 1e-10,
        step["carryIndex_"] == (0 if (start + end) / 2 < knot else 1),
        step["projection_"] == (abs(start - 182) < 1e-10),
    ]


def event_case(check, name, p, request):
    anchors = {
        round(node["timeYears_"] * 365): node
        for node in p["timeGrid_"]
        if node["calendarDate_"] is not None
    }
    knot = request["carry"][0][0]
    conditions = [
        anchors[knot]["isCarryKnot_"],
        anchors[182]["isExerciseDate_"],
        anchors[365]["isMaturity_"],
        all(
            b["timeYears_"] > a["timeYears_"]
            for a, b in zip(p["timeGrid_"], p["timeGrid_"][1:])
        ),
        sum(abs(node["timeYears_"] * 365 - knot) < 1e-10 for node in p["timeGrid_"])
        == 1,
    ]
    if knot == 182:
        conditions.append(anchors[182]["isCarryKnot_"])
    for step in p["steps_"]:
        conditions += step_conditions(p, knot, step)
    check(name + "-events", conditions, dict(anchors=anchors, steps=len(p["steps_"])))


def events(check, outputs, requests):
    for name in [n for n in outputs if n.startswith("N4-")]:
        event_case(check, name, outputs[name]["plan"], requests[name])


def deterministic_expected(name):
    swapped = "-1-" in name
    maturity = name.endswith("terminal") or swapped
    return (
        20 * math.exp(0.2 * (365 - 2 * 137) / 365)
        if maturity and swapped
        else (
            20 * math.exp(-0.2 * (365 - 2 * 137) / 365)
            if maturity
            else 20 * math.exp(0.2 * 137 / 365)
        )
    )


def refinement_meshes(run, i):
    run(f"N6-{i}-space", [f"N6-{i}-space-{n}" for n in [401, 801, 1601]])
    run(f"N6-{i}-time", [f"N6-{i}-time-{n}" for n in [1000, 2000, 4000]])
    run(f"N7-{i}-domain", [f"N7-{i}-domain-{n}" for n in [400, 600, 800]])
    run(
        f"N7-{i}-penalty",
        [f"N7-{i}-penalty-{p}" for p in [1e7, 1e8, 1e9]],
        equal=i in [0, 3],
    )


def refinement_bumps(run, i):
    for n in [801, 1601]:
        run(f"N7-{i}-spot-{n}", [f"N7-{i}-spot-{n}-{c}" for c in [4, 2]])
    run(f"N7-{i}-parameter", [f"N7-{i}-parameter-{s}" for s in [1, 0.5, 0.25]])


def extra_audits(directory, checks):
    for filename, name in [
        ("n9a-audit.json", "N9a-layers"),
        ("supplement.json", "supplement"),
    ]:
        value = optional_json(directory / filename)
        if value is not None:
            checks[name] = value


def premiums(check, outputs, requests):
    for name in [n for n in outputs if n.startswith("N3-")]:
        r = outputs[name]
        check(
            name + "-exercise-premium",
            [
                r["price"] is not None and r["price"] > scalar_price(requests[name]),
                r["scenarios"][0]["evaluationMembership_"] == "CONTINUATION",
            ],
            dict(
                price=r["price"],
                european=scalar_price(requests[name]),
                membership=r["scenarios"][0]["evaluationMembership_"],
            ),
        )


def empty_regions(check, outputs, checks, supplement):
    for name in [n for n in outputs if n.startswith("N2-")]:
        r = outputs[name]
        nonterminal = [
            level for level in r["base_levels"] if level["kind_"] != "TERMINAL"
        ]
        check(
            name + "-base-empty-regions",
            [all(level["regionCount_"] == 0 for level in nonterminal)],
        )
        checks[name + "-scenario-empty-regions"] = (
            supplement["checks"][name]
            if supplement
            else dict(
                status="INCOMPLETE",
                evidence="Bump summaries do not retain region histories; separate audit required",
            )
        )


def deterministic(check, outputs):
    for name in [n for n in outputs if n.startswith("N5a-")]:
        expected = deterministic_expected(name)
        r = outputs[name]
        check(
            name,
            [
                r["price"] is not None and abs(r["price"] - expected) / 100 <= 1e-4,
                all(
                    row["status_"] == "UNAVAILABLE_DEGENERATE_DIFFUSION"
                    for row in r["risks"]
                ),
            ],
            dict(expected=expected, actual=r["price"]),
        )


def deep_exercise(check, outputs):
    for name in ["N5c-0", "N5c-1"]:
        r = outputs[name]
        expected = [0.5, -1, 0, 0, 0, 0]
        check(
            name,
            [
                r["status"] == "COMPLETE",
                within(differences(normalized(r), expected)),
                all(
                    row["status_"] == "AVAILABLE_EXERCISE_REGION" for row in r["risks"]
                ),
            ],
            normalized(r),
        )


def sequence(check, outputs, name, names, equal=False):
    values = [normalized(outputs[n]) for n in names]
    diff = differences(values[-1], values[-2])
    available = all(all(v is not None for v in value) for value in values)
    check(
        name,
        [
            available,
            within(diff, 0.5),
            not equal or all(v == values[0] for v in values),
        ],
        dict(fixtures=names, values=values, last_difference=diff),
    )


def cn(check, outputs):
    for i, base in enumerate(["N1-0-100-0.03", "N3-0-100"]):
        check(
            f"N8-{i}-implicit-comparison",
            [
                within(
                    differences(
                        normalized(outputs[f"N8-{i}"]), normalized(outputs[base])
                    )
                )
            ],
        )


def failures(check, outputs):
    for name, expected in [
        ("N9b-1-100", "LCP_RESIDUAL_LIMIT"),
        ("N9b-100000000.0-1", "MAX_ITERATIONS"),
    ]:
        r = outputs[name]
        check(
            name,
            [
                r["status"] == "BASE_FAILED",
                r["price"] is None,
                r["scenarios"][0]["solverStatus_"] == expected,
                all(row["value_"] is None for row in r["risks"]),
            ],
            r["scenarios"][0],
        )
    check("N9b-control", [outputs["N9b-100000000.0-100"]["price"] is not None])
    r = outputs["N9c"]
    s = r["scenarios"][0]
    check(
        "N9c-measured-failure",
        [
            r["status"] == "BASE_FAILED",
            s["solverStatus_"] == "LINEAR_SOLVE_FAILURE",
            s["maxLinearBackwardError_"] > 1e-30,
            r["price"] is None,
        ],
        s,
    )


def work_limits(check, outputs):
    for cap, status, work in [
        (1326, "COMPLETE", 1326),
        (1325, "WORK_LIMIT", 1287),
        (390, "WORK_LIMIT", 390),
        (78, "WORK_LIMIT", 78),
        (77, "BASE_FAILED", 39),
    ]:
        r = outputs[f"N9d-{cap}"]
        # Coarse grids can be PARTIAL from the stability screen at the unlimited cap.
        statuses = ["COMPLETE", "PARTIAL"] if cap == 1326 else [status]
        check(
            f"N9d-{cap}",
            [r["status"] in statuses, r["execution"]["workUnits_"] == work],
            r["execution"],
        )


def degeneracy(check, outputs):
    r = outputs["N10b-0"]
    check(
        "N10b-zero",
        [
            r["status"] == "PARTIAL",
            len(r["scenarios"]) == 1,
            all(
                row["status_"] == "UNAVAILABLE_DEGENERATE_DIFFUSION"
                for row in r["risks"]
            ),
        ],
    )
    check(
        "N10b-small",
        [outputs["N10b-1e-08"]["risks"][2]["status_"] == "UNAVAILABLE_DOMAIN_BOUNDARY"],
    )
    for spot in [0.25, 399.75]:
        r = outputs[f"N10c-{spot}"]
        check(
            f"N10c-{spot}",
            [
                all(
                    row["status_"] == "UNAVAILABLE_DOMAIN_BOUNDARY"
                    for row in r["risks"][:2]
                )
            ],
            r["risks"],
        )
    check(
        "N10-upwind",
        [any(row["upwindChanged_"] for row in outputs["N10-upwind"]["scenarios"])],
    )


def gamma_sequences(check, outputs):
    for half in [0, 1]:
        names = [f"N11-space-{half}-{n}" for n in [401, 801, 1601]]
        values = [normalized(outputs[n]) for n in names]
        diff = differences(values[-1], values[-2])
        check(
            f"N11-space-{half}",
            [
                all(value[2] is not None for value in values),
                diff[2] is not None and diff[2] <= 0.01,
            ],
            dict(fixtures=names, values=values, last_difference=diff),
        )


def gamma_methods(check, outputs):
    for spot in [100, 100.125]:
        r = outputs[f"N11-admit-{spot}"]
        gamma = r["risks"][1]
        check(
            f"N11-admit-{spot}-method",
            [
                gamma["fineBump_"] == 0.5,
                gamma["coarseBump_"] == 1,
                gamma["method_"] == "FiniteScaleCentralDifference",
                gamma["interpretation_"] == "FINITE_SCALE_MODEL_APPROXIMATION",
                gamma["value_"] is not None and gamma["value_"] != 0,
            ],
            gamma,
        )


def carry_axes(check, outputs, requests):
    for name in [n for n in outputs if n.startswith("N12-")]:
        r = outputs[name]
        expected = [
            "Delta",
            "Gamma",
            "Vega",
            "Rate[0]",
            "DividendYield[0]",
            "Rate[1]",
            "DividendYield[1]",
        ]
        conditions = [[row["riskId_"] for row in r["risks"]] == expected]
        evaluation = datetime.date(2026, 10, 18)
        for i, segment in enumerate(requests[name]["carry"]):
            start = 0 if i == 0 else requests[name]["carry"][i - 1][0]
            for j in [0, 1]:
                row = r["risks"][3 + 2 * i + j]
                conditions += [
                    row["segmentIndex_"] == i,
                    row["parameterValue_"] == segment[1 + j],
                    row["startDate_"]
                    == (evaluation + datetime.timedelta(days=start)).isoformat(),
                    row["endDate_"]
                    == (evaluation + datetime.timedelta(days=segment[0])).isoformat(),
                ]
        check(name + "-axes", conditions, r["risks"])


def summarize(directory):
    requests = fixtures()
    outputs = {name: load(directory, name) for name in requests}
    checks = {}
    supplement = optional_json(directory / "supplement.json")

    def check(name, conditions, evidence=None):
        checks[name] = dict(
            status="PASS" if all(conditions) else "FAIL", evidence=evidence
        )

    general(check, outputs, requests)
    events(check, outputs, requests)
    premiums(check, outputs, requests)
    empty_regions(check, outputs, checks, supplement)
    deterministic(check, outputs)
    deep_exercise(check, outputs)
    run = partial(sequence, check, outputs)
    for i in range(4):
        refinement_meshes(run, i)
        refinement_bumps(run, i)
    cn(check, outputs)
    failures(check, outputs)
    work_limits(check, outputs)
    degeneracy(check, outputs)
    gamma_sequences(check, outputs)
    gamma_methods(check, outputs)
    carry_axes(check, outputs, requests)
    extra_audits(directory, checks)
    write_json(directory / "audits.json", checks)
    failed = [name for name, row in checks.items() if row["status"] == "FAIL"]
    print("audits", len(checks), "failures", len(failed))
    print("\n".join(failed))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=pathlib.Path)
    summarize(parser.parse_args().directory)

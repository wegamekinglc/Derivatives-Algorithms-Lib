import copy
import math

BASE = dict(
    evaluation=[2026, 10, 18],
    days=365,
    spot=100.0,
    vol=0.2,
    call=False,
    american=False,
    carry=[[365, 0.05, 0.03]],
    exercise=[365],
)


def european(add):
    for call in [False, True]:
        for spot in [80, 100, 120]:
            for q in [0, 0.03]:
                add(
                    f"N1-{int(call)}-{spot}-{q}",
                    call=call,
                    spot=spot,
                    carry=[[365, 0.05, q]],
                )
    add(
        "N1-default",
        call=True,
        spot=120,
        carry=[[365, 0.05, 0]],
        nodes=401,
        steps=500,
        default_upper=True,
        default_caps=True,
    )


def non_dividend(add):
    for spot in [80, 100, 120]:
        add(
            f"N2-{spot}",
            call=True,
            spot=spot,
            carry=[[365, 0.05, 0]],
            american=True,
            exercise=[],
        )
    add(
        "N2-piecewise",
        call=True,
        american=True,
        exercise=[],
        carry=[[137, 0.02, 0], [365, 0.06, 0]],
    )


def exercise(add):
    for call in [False, True]:
        for spot in [100, 120]:
            add(
                f"N3-{int(call)}-{spot}",
                call=call,
                spot=spot,
                american=True,
                exercise=[],
                carry=[[365, 0.05, 0.08 if call else 0]],
            )
        for spot in [100, 120]:
            for knot in [137, 182]:
                add(
                    f"N4-{int(call)}-{spot}-{knot}",
                    call=call,
                    spot=spot,
                    exercise=[182, 365],
                    carry=[[knot, 0.02, 0.01], [365, 0.06, 0.03]],
                )


def deterministic(add):
    for swap in [False, True]:
        rate = 0.2 if swap else -0.2
        for style in ["bermudan", "american", "terminal"]:
            add(
                f"N5a-{int(swap)}-{style}",
                spot=80,
                vol=0,
                carry=[[137, rate, rate], [365, -rate, -rate]],
                american=style == "american",
                exercise=(
                    []
                    if style == "american"
                    else [365] if style == "terminal" else [137, 365]
                ),
            )


def short_dates(add):
    for year, maturity_days in [(2028, 2), (2027, 2)]:
        for call in [False, True]:
            for spot in [100, 100.0125]:
                add(
                    f"N5b-{year}-{int(call)}-{spot}",
                    evaluation=[year, 2, 28],
                    days=maturity_days,
                    call=call,
                    spot=spot,
                    nodes=16001,
                    carry=[[2, 0.05, 0.03]],
                    exercise=[2],
                )
    for american in [False, True]:
        add(
            f"N5c-{int(american)}",
            spot=50,
            american=american,
            exercise=[] if american else [0, 182, 365],
            carry=[[365, 0.05, 0]],
        )


def failures(add):
    add(
        "N9a",
        american=True,
        exercise=[],
        nodes=7,
        steps=8,
        carry=[[365, 0.05, 0]],
        delta=False,
        gamma=False,
        carry_risk=False,
    )
    for penalty, iterations in [(1, 100), (100000000.0, 100), (100000000.0, 1)]:
        add(
            f"N9b-{penalty}-{iterations}",
            american=True,
            exercise=[],
            spot=80,
            vol=0,
            carry=[[137, 0.1, 0.1], [365, -0.01, -0.01]],
            nodes=41,
            steps=500,
            penalty=penalty,
            iterations=iterations,
        )
    add("N9c", nodes=401, steps=500, linear=1e-30)
    for work in [1326, 1325, 390, 78, 77]:
        add(f"N9d-{work}", nodes=41, steps=2, rannacher=0, work=work)


def stress(add):
    for vol in [0, 1e-08]:
        add(f"N10b-{vol}", vol=vol)
    for spot in [0.25, 399.75]:
        add(f"N10c-{spot}", spot=spot, h=0.5)
    for call in [False, True]:
        for i, (spot, vol, rate, q) in enumerate(
            [
                (20, 0.2, 0.05, 0),
                (300, 0.2, 0.05, 0),
                (100, 1.0, 0.05, 0.03),
                (100, 0.2, -0.05, 0.03),
                (100, 0.2, 0.2, -0.1),
            ]
        ):
            add(
                f"N10d-{int(call)}-{i}",
                call=call,
                spot=spot,
                vol=vol,
                carry=[[365, rate, q]],
                upper=800 if vol == 1.0 else 400,
                nodes=3201 if vol == 1.0 else 1601,
            )
    add("N10-upwind", vol=math.sqrt(0.05), nodes=401, carry=[[365, 0.05, 0]])


def gamma(add):
    for spot in [100, 100.125]:
        add(f"N11-reject-{spot}", spot=spot, h=0.125)
        add(f"N11-admit-{spot}", spot=spot, h=0.5)
    for half in [False, True]:
        for nodes in [401, 801, 1601]:
            add(
                f"N11-space-{int(half)}-{nodes}",
                nodes=nodes,
                spot=100 + (200 / (nodes - 1) if half else 0),
            )


def carry(add, result):
    for knot in [137, 182]:
        for bump in [0.0001, 5e-05]:
            add(
                f"N12-{knot}-{bump}",
                **dict(result[f"N4-0-100-{knot}"], carry_bump=bump),
            )


def mesh(add, base, i):
    for nodes in [401, 801, 1601]:
        add(f"N6-{i}-space-{nodes}", **dict(base, nodes=nodes, steps=8000))
    for steps in [1000, 2000, 4000]:
        add(f"N6-{i}-time-{steps}", **dict(base, nodes=1601, steps=steps, h=0.5))
    for upper, nodes in [(400, 1601), (600, 2401), (800, 3201)]:
        add(f"N7-{i}-domain-{upper}", **dict(base, nodes=nodes, upper=upper, h=0.5))


def bumps(add, base, i):
    for penalty in [1e7, 1e8, 1e9]:
        add(f"N7-{i}-penalty-{penalty}", **dict(base, penalty=penalty))
    for nodes in [801, 1601]:
        for cells in [4, 2]:
            add(
                f"N7-{i}-spot-{nodes}-{cells}",
                **dict(base, nodes=nodes, h=cells * 400 / (nodes - 1)),
            )
    for scale in [1, 0.5, 0.25]:
        add(
            f"N7-{i}-parameter-{scale}",
            **dict(base, vol_bump=0.001 * scale, carry_bump=0.0001 * scale),
        )


def refinements(add, result):
    controls = ["N1-0-100-0.03", "N3-0-100", "N3-1-100", "N4-0-100-137"]
    for i, name in enumerate(controls):
        mesh(add, result[name], i)
        bumps(add, result[name], i)
    for i, name in enumerate(controls[:2]):
        add(f"N8-{i}", **dict(result[name], theta=0.5, nodes=401, steps=5000))


def fixtures():
    result = {}

    def add(name, **overrides):
        result[name] = dict(copy.deepcopy(BASE), **overrides)

    european(add)
    non_dividend(add)
    exercise(add)
    deterministic(add)
    short_dates(add)
    refinements(add, result)
    failures(add)
    stress(add)
    gamma(add)
    carry(add, result)
    return result

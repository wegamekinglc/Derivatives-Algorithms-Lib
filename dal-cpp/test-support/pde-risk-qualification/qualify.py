#!/usr/bin/env python3
import argparse
import copy
import gzip
import hashlib
import json
import math
import pathlib

from fixtures import BASE, fixtures

HARD = [1e-4, 2e-3, 2e-2, 3e-3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def invoke(driver, directory, name, mode, request):
    source = directory / (name + ".input.json")
    expected = json.loads(source.read_text())
    if expected != request:
        raise RuntimeError("capture input differs: " + name)
    return load(directory, name)


def load(directory, name):
    with gzip.open(directory / (name + ".json.gz"), "rt") as stream:
        return json.load(stream)


def normalized(output):
    return [None if output.get("price") is None else output["price"] / 100] + [
        (
            None
            if r["value_"] is None
            else r["value_"]
            * (
                100
                if r["riskId_"] == "Gamma"
                else 1 if r["riskId_"] == "Delta" else 0.01
            )
        )
        for r in output.get("risks", [])
    ]


def normalize_reference(values):
    return [
        v * (0.01 if i == 0 or i >= 3 else 100 if i == 2 else 1)
        for i, v in enumerate(values)
    ]


def limits(count):
    return HARD + [3e-3] * (count - 4)


def differences(a, b):
    if len(a) != len(b):
        return None
    return [None if x is None or y is None else abs(x - y) for x, y in zip(a, b)]


def within(diff, fraction=1, strict=False):
    return diff is not None and all(
        x is not None and (x < fraction * t if strict else x <= fraction * t)
        for x, t in zip(diff, limits(len(diff)))
    )


def scalar_price(request):
    t = request.get("days", 365) / 365
    start = 0
    rate = dividend = 0
    for end, r, q in request["carry"]:
        rate += r * (end - start) / 365
        dividend += q * (end - start) / 365
        start = end
    spot, vol = request.get("spot", 100), request.get("vol", 0.2)
    sd = vol * math.sqrt(t)
    d2 = (math.log(spot / 100) + rate - dividend - 0.5 * sd * sd) / sd
    d1 = d2 + sd
    sign = 1 if request.get("call", False) else -1
    cdf = lambda x: (1 + math.erf(x / math.sqrt(2))) / 2
    return sign * (
        spot * math.exp(-dividend) * cdf(sign * d1)
        - 100 * math.exp(-rate) * cdf(sign * d2)
    )


def bumped(request, axis, offset):
    r = copy.deepcopy(request)
    if axis in ["spot", "vol"]:
        r[axis] = r.get(axis, 100 if axis == "spot" else 0.2) + offset
    else:
        family, index = axis
        if index == "parallel":
            for segment in r["carry"]:
                segment[family] += offset
        else:
            r["carry"][index][family] += offset
    return r


def derivatives(price, request, spot_bump, parameter_bump, carry_bump):
    base = price(request)
    result = [base]
    low, high = price(bumped(request, "spot", -spot_bump)), price(
        bumped(request, "spot", spot_bump)
    )
    result += [(high - low) / (2 * spot_bump), (high - 2 * base + low) / spot_bump**2]
    for axis, bump in [("vol", parameter_bump)] + [
        (family_index, carry_bump)
        for i in range(len(request["carry"]))
        for family_index in [(1, i), (2, i)]
    ]:
        result.append(
            (price(bumped(request, axis, bump)) - price(bumped(request, axis, -bump)))
            / (2 * bump)
        )
    return result


def required_vector(request):
    upper = request.get("upper", 400)
    if request.get("default_upper", False):
        upper = 4 * max(request.get("spot", 100), 100)
    return [
        (
            upper / (request.get("nodes", 1601) - 1) * 2
            if "h" not in request
            else request["h"]
        ),
        request.get("vol_bump", 0.001),
        request.get("carry_bump", 0.0001),
    ]


def reference_name(name):
    from referenceplan import binding

    return binding(fixtures()[name])


def numerical_verdict(output, reference):
    if output.get("status") != "COMPLETE":
        return dict(
            status="FAIL",
            reason="mandatory smooth risk unavailable or base/preflight failed",
        )
    if reference is None or reference["status"] != "PASS":
        return dict(
            status="INCOMPLETE",
            reason="independent reference not qualified for actual bump scales",
        )
    diff = differences(normalized(output), reference["normalized"])
    return dict(difference=diff, status="PASS" if within(diff) else "FAIL")


def is_numerical(name):
    return name.startswith(
        (
            "N1-",
            "N2-",
            "N3-",
            "N4-",
            "N5b-",
            "N6-",
            "N7-",
            "N8-",
            "N10d-",
            "N11-admit",
            "N11-space",
            "N12-",
        )
    )


def special_verdict(name, output, refs):
    if name == "N1-default":
        return dict(
            status="PASS" if output.get("plan", {}).get("upper_") == 480 else "FAIL"
        )
    if name.startswith("N11-reject"):
        return dict(
            status=(
                "PASS"
                if "InvalidEarlyExercisePdeRiskRequest" in output.get("error", "")
                else "FAIL"
            )
        )
    if is_numerical(name):
        key = reference_name(name)
        return dict(numerical_verdict(output, refs.get(key)), reference=key)
    if "error" in output:
        return dict(status="FAIL", reason="unexpected preflight/error")
    return dict(status="INCOMPLETE", reason="contract audit required")


def analyze_case(directory, name, refs):
    path = directory / (name + ".json.gz")
    if not path.exists():
        return dict(status="INCOMPLETE", reason="not executed on this identity")
    output = load(directory, name)
    row = dict(
        normalized=normalized(output),
        aggregate=output.get("status"),
        error=output.get("error"),
    )
    if "risks" in output:
        row.update(
            risks=output["risks"],
            scenarios=output["scenarios"],
            execution=output["execution"],
            plan_id=output["plan"]["planId_"],
            actual_steps=output["plan"]["plannedSteps_"],
        )
    row.update(special_verdict(name, output, refs))
    return row


def analyze(directory):
    refs = json.loads((directory / "references.json").read_text())
    rows = {name: analyze_case(directory, name, refs) for name in fixtures()}
    write_json(directory / "summary.json", rows)
    counts = {}
    for name, row in rows.items():
        group = counts.setdefault(name.split("-")[0], {})
        group[row["status"]] = group.get(row["status"], 0) + 1
    print(json.dumps(counts, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=pathlib.Path)
    analyze(parser.parse_args().directory)

"""Exact saved-output errors from inherited outward Gaussian coefficient means."""

import argparse
import hashlib
import json
import math
import statistics
from dataclasses import dataclass
from fractions import Fraction as F
from pathlib import Path

INPUTS = {
    ".codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md": "c2ff68e1a696b620682b896ca9b292ec6d577bf92cadb3597991b427267733d5",
    "dal-cpp/tests/script/fixtures/xccy_bias_frozen_inputs.json": "5c2f3d48861e173185219bd32fad7cbf49a6c9e92918586b9a7bc0bd8108837a",
    "acceptance-basket/APPROXIMATE-x1.command.json": "0671ffaf92d4236bd18480fbc2e369aad0086806c99154f0c68a7c2f19649736",
    "acceptance-basket/APPROXIMATE-x1.json": "a5c25f86fcb8e182ec8577f154b19b325c8a54475003a5edd95ccddc9f9e5adf",
    "acceptance-basket/APPROXIMATE-x2.command.json": "767170acadd6955b6356502742178d8c9aa49e67b9a5733771140f8ec5ebe1d6",
    "acceptance-basket/APPROXIMATE-x2.json": "fd2210b5fc51316b4dc9b8923c06d7caa51a3a9b02d59195a5597c7640ae980a",
    "acceptance-basket/APPROXIMATE-x4.command.json": "d49903ea2d8f74ef6ed6f84fcbfd553f58ccce6e3a7ccf43a187da9eb3bbc156",
    "acceptance-basket/APPROXIMATE-x4.json": "3a025dece7bdace32145d99805454daf8b5bea18710616b311da7b37b9e11687",
    "acceptance-basket/EXACT-x1.command.json": "56354459f65e57e7a4d3c96e98016c5ea44ea3352a42352ef7da4dc0e89fd00f",
    "acceptance-basket/EXACT-x1.json": "7dcb4aafa45ac81226113a92bd144440030105954e8a76994962c9e21051eac3",
    "acceptance-basket/EXACT-x2.command.json": "d0210de3a6d64bac0c50cb3d61d54ffb6688f0fc6944a3ef6c4fff63ca9eb9de",
    "acceptance-basket/EXACT-x2.json": "657b4decdec82c10709261fd7a4caf2afe50f88216b32582a53a4368cb3fbea6",
    "acceptance-basket/EXACT-x4.command.json": "df715a8ae52738b7680b829b09912bc861eb13c0c5e2be712a3b26e45b991c7f",
    "acceptance-basket/EXACT-x4.json": "3862298a985e92941377a15b66fb9159553bfbcf316e678e16c340451e085cdb",
    "acceptance-basket/numerical-report-strict.json": "cca7aac898607d6b2eef7f5d477e259cbff7fd1f167399cb55da35bddbc51327",
    "acceptance-basket/oracle.json": "ef53e98e1dae547439f0bfc983974256f8b35b665be730e834ef0b763897ba5c",
    "acceptance-bias/certificate-final.json": "2f6666deed5b704edfc37014f99a8ac06e7bb537edd4bbc12754f480cc59ef49",
    "dal-cpp/tests/script/xccy_acceptance_bias_qualification.py": "6d476da8f2fa12ce71193d4fba3487d802e570ec70bad12f0f54d6b8a7428d63",
    "dal-cpp/tests/script/xccy_acceptance_bias_cashflows.py": "ac30858cab4b7eb2bb2823b055405c2fa6e721b5b7a2ac050cf362b7e94c54c4",
    "dal-cpp/tests/script/xccy_acceptance_interval.py": "99c080542665b9bf61f669a52cf080d404d6fe9542e07431df8c9529e4fce590",
    "dal-cpp/tests/script/xccy_acceptance_extended.py": "9e4d1df79bed00be9c9930a590bd53e5f498aecf58f84f18948b9361211a1b84",
    "dal-cpp/tests/script/xccy_acceptance_bias_intervals.py": "968a69f73f8886d35dade06a14a2740ec8b5bd651499f6e39390b2f783b97985",
}
GLOBAL_FLAGS = (
    "financial_bias_bound_qualified",
    "actual_joint_law_qualified",
    "caller_binding_qualified",
    "original_numerical_pass",
)
REPORT = "acceptance-basket/numerical-report-strict.json"
ACCEPTED = "acceptance-bias/certificate-final.json"
FIXTURE = "dal-cpp/tests/script/fixtures/xccy_bias_frozen_inputs.json"
ORACLE = "acceptance-basket/oracle.json"


def require(condition, message):
    if not condition:
        raise ValueError(message)


@dataclass(frozen=True)
class Bounds:
    lo: F
    hi: F

    def __post_init__(self):
        require(
            type(self.lo) is F and type(self.hi) is F and self.lo <= self.hi,
            "ordered rational enclosure",
        )

    def __add__(self, other):
        return Bounds(self.lo + other.lo, self.hi + other.hi)

    def __neg__(self):
        return Bounds(-self.hi, -self.lo)

    def __sub__(self, other):
        return self + -other

    def __mul__(self, other):
        corners = [a * b for a in (self.lo, self.hi) for b in (other.lo, other.hi)]
        return Bounds(min(corners), max(corners))

    def positive_divide(self, denominator):
        require(denominator.lo > 0, "strictly positive Gaussian annuity")
        return self * Bounds(1 / denominator.hi, 1 / denominator.lo)

    def absolute(self):
        lo = F(0) if self.lo <= 0 <= self.hi else min(abs(self.lo), abs(self.hi))
        return Bounds(lo, max(abs(self.lo), abs(self.hi)))


def binary_endpoint(value):
    require(
        isinstance(value, list) and len(value) == 4,
        "finite encoded binary endpoint, not a point",
    )
    require(all(type(x) is int for x in value), "integer binary endpoint")
    sign, mantissa, exponent, bits = value
    require(
        sign in (0, 1) and mantissa >= 0 and abs(exponent) <= 4096,
        "finite binary endpoint domain",
    )
    require(bits == mantissa.bit_length(), "binary endpoint bit count")
    require(mantissa != 0 or value == [0, 0, 0, 0], "canonical finite zero")
    return (-1 if sign else 1) * F(mantissa) * F(2) ** exponent


def inherited_bounds(encoded):
    require(isinstance(encoded, dict), "inherited interval, not an oracle point")
    result = Bounds(
        binary_endpoint(encoded["binary_lower"]),
        binary_endpoint(encoded["binary_upper"]),
    )
    require(
        F(encoded["decimal_lower"])
        <= result.lo
        <= result.hi
        <= F(encoded["decimal_upper"]),
        "outward inherited decimal enclosure",
    )
    return result


def gaussian_measures(a, b, coupon):
    require(type(coupon) is F, "exact coupon")
    return {"Price": b + Bounds(coupon, coupon) * a, "par": b.positive_divide(-a)}


def compare_payload(value, reference, budget, measure, units):
    require(
        (measure, units) in (("Price", "USD"), ("par", "decimal-spread")),
        "measure units",
    )
    require(
        type(value) is F and type(budget) is F and budget > 0,
        "exact payload and positive budget",
    )
    signed = Bounds(value, value) - reference
    absolute = signed.absolute()
    status = "within" if absolute.hi <= budget else "inconclusive"
    if absolute.lo > budget:
        status = "RED"
    return {"signed": signed, "absolute": absolute, "status": status}


def rational_text(value, upward, places=35):
    scaled = value * 10**places
    rounded = (
        -(-scaled.numerator // scaled.denominator)
        if upward
        else scaled.numerator // scaled.denominator
    )
    digits = str(abs(rounded)).rjust(places + 1, "0")
    return ("-" if rounded < 0 else "") + digits[:-places] + "." + digits[-places:]


def encode(value):
    if isinstance(value, Bounds):
        return {
            "lower": encode(value.lo),
            "upper": encode(value.hi),
            "decimal_lower": rational_text(value.lo, False),
            "decimal_upper": rational_text(value.hi, True),
        }
    if isinstance(value, F):
        return {
            "numerator": str(value.numerator),
            "denominator": str(value.denominator),
        }
    if isinstance(value, dict):
        return {k: encode(v) for k, v in value.items()}
    if isinstance(value, (tuple, list)):
        return [encode(v) for v in value]
    return value


def load_bound_json(path, expected):
    raw = path.read_bytes()
    require(
        hashlib.sha256(raw).hexdigest() == expected,
        "frozen input identity: " + str(path),
    )
    return json.loads(raw, parse_float=str)


def read_inputs(root):
    data = {}
    for name, digest in INPUTS.items():
        path = root / name
        if name.endswith(".json"):
            data[name] = load_bound_json(path, digest)
        else:
            require(
                hashlib.sha256(path.read_bytes()).hexdigest() == digest,
                "inherited source identity: " + name,
            )
    accepted = data[ACCEPTED]
    require(
        all(accepted.get(key) is False for key in GLOBAL_FLAGS),
        "preserved global flags",
    )
    require(accepted["precision_bits"] == 236, "inherited outward precision")
    require(accepted["fixture_sha256"] == INPUTS[FIXTURE], "inherited fixture binding")
    return data


def complete_rows(keys):
    required = {
        (mode, factor, member, measure)
        for mode in ("EXACT", "APPROXIMATE")
        for factor in (1, 2, 4)
        for member in range(3)
        for measure in ("Price", "par")
    }
    require(
        len(keys) == 36 and set(keys) == required,
        "complete unique 36 saved output rows",
    )


def binary64(token):
    value = float(token)
    require(math.isfinite(value), "finite saved binary64")
    return F.from_float(value)


def payload_record(token, reference, budget, measure, units):
    value = binary64(token)
    return {
        "token": str(token),
        "binary64_hex": float(token).hex(),
        "exact": value,
        **compare_payload(value, reference, budget, measure, units),
    }


def bind_reference(case, profile, raw, context, member):
    for key in ("fit", "id", "dimension", "source", "instrument", "mapping"):
        require(case[key] == profile[key], "Gaussian reference identity: " + key)
    require(
        profile["model"]
        == case["model"]["identity"]
        == raw["result"]["model"]
        == context["model"],
        "Gaussian model identity",
    )
    allocation = raw["allocation"]["members"][member]
    require(
        all(
            allocation[key] == case[key] for key in ("source", "instrument", "mapping")
        ),
        "member/reference identity",
    )
    for key in (
        "id",
        "source",
        "binding",
        "context",
        "mapping",
        "instrument",
        "measure",
    ):
        require(
            allocation[key] == context["members"][member][key],
            "oracle/capture identity: " + key,
        )
    require(
        allocation["D"] == case["dimension"]
        and allocation["grid"] == case["grid"]
        and allocation["coordinates"] == case["coordinates"],
        "full geometry identity",
    )
    require(
        allocation["id"] == "independent-staged-" + str(member), "ordered saved member"
    )
    return allocation


def admit_capture(raw, command, run):
    require(
        command["exit_code"] == 0
        and raw["mode"] == run["mode"]
        and raw["factor"] == run["factor"],
        "successful saved capture",
    )
    require(
        (
            raw["allocation"]["seed"],
            raw["allocation"]["precision"],
            raw["allocation"]["phase"],
        )
        == (1024, "Precise", "HeldOut"),
        "saved request scope",
    )
    require(
        len(raw["result"]["members"]) == len(run["members"]) == 3,
        "complete captured/report members",
    )


def admit_saved_value(captured, old, measure, factor):
    scale = 0.01 if measure == "Price" else 0.0001
    require(
        old["measure"] == measure and float(old["scale"]) == scale,
        "frozen output scale and unit",
    )
    require(
        len(captured["rows"]) == 16 and captured["batches"] == 16,
        "sixteen captured batches",
    )
    require(captured["paths"] == 16 * 16384 * factor, "fixed captured path prefix")
    bm, am = (
        statistics.mean(float(row[j]) for row in captured["rows"]) for j in (0, 1)
    )
    derived = bm + 0.001 * am if measure == "Price" else -bm / am
    require(
        derived.hex() == float(old["mean"]).hex(),
        "saved report recomposed from captured coefficients",
    )


def fitted_oracle(data, mode):
    return next(fit for fit in data[ORACLE]["fits"] if fit["mode"] == mode)


def certificate_row(data, run_index, member_index, measure_index):
    run = data[REPORT]["runs"][run_index]
    mode, factor = run["mode"], run["factor"]
    case_index = (0 if mode == "EXACT" else 3) + member_index
    case = data[FIXTURE]["cases"][case_index]
    profile = data[ACCEPTED]["profiles"][case_index]
    raw_name = f"acceptance-basket/{mode}-x{factor}.json"
    raw = data[raw_name]
    command = data[raw_name.replace(".json", ".command.json")]
    admit_capture(raw, command, run)
    oracle = fitted_oracle(data, mode)
    allocation = bind_reference(case, profile, raw, oracle["context"], member_index)
    captured = raw["result"]["members"][member_index]
    member = run["members"][member_index]
    require(
        member["id"] == captured["id"] == allocation["id"],
        "saved member/report identity",
    )
    centers = profile["baseline"]["centers"]
    a, b = (inherited_bounds(centers[key]) for key in ("A", "B"))
    references = gaussian_measures(a, b, F.from_float(0.001))
    old = member["measures"][measure_index]
    measure, units = (("Price", "USD"), ("par", "decimal-spread"))[measure_index]
    admit_saved_value(captured, old, measure, factor)
    reference = references[measure]
    scale = F(1, 100) if measure == "Price" else F(1, 10000)
    budget = scale / 4
    observed = payload_record(old["mean"], reference, budget, measure, units)
    result = {
        "mode": mode,
        "factor": factor,
        "member_index": member_index,
        "member": member["id"],
        "dimension": case["dimension"],
        "measure": measure,
        "units": units,
        "reference": reference,
        "gaussian_annuity": -a,
        "quarter_budget": budget,
        "s_measure": scale,
        "saved_report": observed,
        "above_s_measure": observed["absolute"].lo > scale,
        "report_pointer": f"/runs/{run_index}/members/{member_index}/measures/{measure_index}/mean",
        "capture_file": raw_name,
        "capture_member_pointer": f"/result/members/{member_index}",
        "report_value_origin": "binary64 recomposition of paired batch coefficient means; par is ratio of means",
        "capture_a_token": str(captured["estimate"]["a"]),
        "capture_b_token": str(captured["estimate"]["b"]),
        "reference_pointer": f"/profiles/{case_index}/baseline/centers",
        "identities": {
            **{k: case[k] for k in ("source", "instrument", "mapping")},
            "model": profile["model"],
            "binding": allocation["binding"],
            "context": allocation["context"],
        },
        "original_flags": {k: old[k] for k in ("parity", "precision", "combined")},
        "original_SE_token": str(old["se"]),
        "original_eFast_token": str(old["numeric_indicator"]),
    }
    if measure == "Price":
        result["direct_capture_Price"] = payload_record(
            captured["estimate"]["mean"], reference, budget, measure, units
        )
        result["capture_equals_report"] = binary64(
            captured["estimate"]["mean"]
        ) == binary64(old["mean"])
    return result


def summarize(rows):
    return {
        "report_rows": len(rows),
        "report_strict_RED": sum(r["saved_report"]["status"] == "RED" for r in rows),
        "report_above_s_measure": sum(r["above_s_measure"] for r in rows),
        "direct_capture_Price_rows": 18,
        "direct_capture_Price_strict_RED": sum(
            r.get("direct_capture_Price", {}).get("status") == "RED" for r in rows
        ),
    }


def all_rows(data):
    rows = [
        certificate_row(data, run, member, measure)
        for run in range(6)
        for member in range(3)
        for measure in range(2)
    ]
    complete_rows(
        [(r["mode"], r["factor"], r["member_index"], r["measure"]) for r in rows]
    )
    return rows


def certify(root):
    data = read_inputs(root)
    require(
        [(r["mode"], r["factor"]) for r in data[REPORT]["runs"]]
        == [
            (mode, factor) for factor in (1, 2, 4) for mode in ("EXACT", "APPROXIMATE")
        ],
        "six ordered runs",
    )
    rows = all_rows(data)
    return encode(
        {
            "schema": "dal.xccy-fixed-output-gaussian-error/1",
            "scope": "36 saved report payloads; also bind the 18 directly captured Price payloads",
            "reference_trust": "inherited PR643 outward Gaussian coefficient expectations, not its IID-grid error theorem",
            "arithmetic": "exact CPython Fraction; display floors/ceilings at 35 decimal places",
            "input_sha256": INPUTS,
            "global_flags": dict.fromkeys(GLOBAL_FLAGS, False),
            "summary": summarize(rows),
            "rows": rows,
        }
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    result = certify(args.root)
    if args.verify:
        require(
            json.loads(args.output.read_text()) == result,
            "canonical fixed-output certificate replay",
        )
    else:
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["summary"], indent=2))


if __name__ == "__main__":
    main()

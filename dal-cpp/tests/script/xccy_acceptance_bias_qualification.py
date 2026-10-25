"""Finite-stream and conditional IID-grid bounds, with separate qualification.

All certified arithmetic uses mpmath interval/libmp directed rounding.
The draw law is an explicit conditional theorem, never a property inferred
from the fixed-seed MRG trace. This module does not run a DAL binary or MC.
"""

import argparse
import hashlib
import json
from functools import lru_cache
from pathlib import Path

import mpmath as mp

from xccy_acceptance_bias_intervals import interval, lower, magnitude, phi, product, require, sum_iv, tail, upper
from xccy_acceptance_bias_intervals import serialize
from xccy_acceptance_bias_cashflows import IndependentTerms, admit_case, term_arguments
from xccy_acceptance_bias_budget import combine_report, read_report

SCALAR_ERROR = "9.006864425e-9"
DENOMINATOR = 4294967088
SUPPORT = "6.25"
COUPON = 0.001
FIXTURE_SHA256 = "5c2f3d48861e173185219bd32fad7cbf49a6c9e92918586b9a7bc0bd8108837a"


def normal_domain():
    m = interval(SUPPORT)
    mills = phi(m) / m
    require(mills.b < (interval(1) / DENOMINATOR - interval(2) ** -54).a, "Mills support proof")
    quantization = interval(2) ** -54 / phi(m)
    require(quantization.b < interval("4.224587095e-8").a, "uniform quantization bound")
    return dict(
        support=m, mills_upper=mills, uniform_quantile_error=quantization, accepted_scalar_error=interval(SCALAR_ERROR)
    )


@lru_cache(None)
def quantile_bracket(denominator, precision):
    require(denominator > 2 and precision >= 180, "finite-grid proof precision/domain")
    left, right, h = interval(0), interval(SUPPORT), interval(1) / denominator
    for _ in range(160):
        midpoint = (left + right) / 2
        q = tail(midpoint)
        if q.a > h.b:
            left = midpoint
        elif q.b < h.a:
            right = midpoint
        else:
            raise ValueError("unresolved inverse-tail bisection")
    require(tail(left).a > h.b and tail(right).b < h.a, "inverse-tail bracket")
    return mp.iv.mpf([left.a, right.b])


def first_variation(a, t):
    def f(z):
        return z * mp.iv.exp(a * z)

    if (magnitude(a) * t).b <= 1:
        return magnitude(f(t) - f(-t))
    require(abs(a).a > 0, "nonzero stationary coordinate")
    stationary = -1 / a
    return magnitude(f(stationary) - f(-t)) + magnitude(f(t) - f(stationary))


def grid_errors(a, denominator):
    t = quantile_bracket(denominator, mp.iv.prec)
    h, c = interval(1) / denominator, interval(denominator) / (denominator - 1)
    m0 = mp.iv.exp(a * a / 2)
    tail0 = tail(t - magnitude(a))
    r0 = (c - 1) + c * tail0
    endpoints = (magnitude(-t * mp.iv.exp(-a * t)) + magnitude(t * mp.iv.exp(a * t))) / 2
    tail1 = -a * tail(t + a) + phi(t + a) + a * tail(t - a) + phi(t - a)
    r1 = (c - 1) * magnitude(a) + c * (h * (first_variation(a, t) + endpoints) / m0 + tail1)
    return dict(r0=upper(r0), r1=upper(r1), tail0=tail0, tail1=tail1, quantile=t)


@lru_cache(None)
def cached_moments(a_bounds, error_bounds, denominator, precision):
    require(mp.iv.prec == precision, "moment precision identity")
    a, error = mp.iv.make_mpf(a_bounds), mp.iv.make_mpf(error_bounds)
    require(error.a >= 0, "nonnegative transform error")
    result = grid_errors(a, denominator)
    m = interval(SUPPORT)
    e = error + normal_domain()["uniform_quantile_error"]
    growth = mp.iv.expm1(magnitude(a) * e)
    extra0 = (1 + result["r0"]) * growth
    extra1 = (1 + result["r0"]) * (m * growth + e * mp.iv.exp(magnitude(a) * e))
    return dict(
        result, r0=upper(result["r0"] + extra0), r1=upper(result["r1"] + extra1), coupling0=extra0, coupling1=extra1
    )


def moment_errors(a, error, denominator=DENOMINATOR):
    return cached_moments(interval(a)._mpi_, interval(error)._mpi_, denominator, mp.iv.prec)


def finite_term(c, k, ell, dc, dk, g, error=SCALAR_ERROR):
    c, k, dc, dk, error = map(interval, (c, k, dc, dk, error))
    require(len(ell) == len(g) and error.a >= 0, "complete finite derivative coordinates")
    m = interval(SUPPORT)
    loading, gl = sum_iv(map(magnitude, ell)), sum_iv(map(magnitude, g))
    base = mp.iv.exp(k + m * loading)
    growth = mp.iv.expm1(error * loading)
    derivative = magnitude(dc + c * dk) + magnitude(c) * m * gl
    return dict(
        value=upper(magnitude(c) * base * growth),
        gradient=upper(base * (derivative * growth + magnitude(c) * error * gl * mp.iv.exp(error * loading))),
        max_value=upper(magnitude(c) * mp.iv.exp(k + (m + error) * loading)),
        max_gradient=upper(
            mp.iv.exp(k + (m + error) * loading) * (magnitude(dc + c * dk) + magnitude(c) * (m + error) * gl)
        ),
        lower_abs=lower(abs(c) * mp.iv.exp(k - (m + error) * loading)),
    )


def law_term(c, k, ell, dc, dk, g):
    c, k, dc, dk = map(interval, (c, k, dc, dk))
    moments = [moment_errors(a, interval(SCALAR_ERROR)) for a in ell]
    factors = [1 + row["r0"] for row in moments]
    total, variance = product(factors), sum_iv(a * a for a in ell)
    normalizer = mp.iv.exp(k + variance / 2)
    gradient_error = magnitude(dc + c * dk) * (total - 1)
    for a, gi, factor, row in zip(ell, g, factors, moments, strict=True):
        others = total / factor
        gradient_error += magnitude(c * gi) * (row["r1"] * others + magnitude(a) * (others - 1))
    return dict(
        value=upper(magnitude(c) * normalizer * (total - 1)),
        gradient=upper(normalizer * gradient_error),
        mean=c * normalizer,
        mean_gradient=normalizer * (dc + c * dk + c * sum_iv(a * gi for a, gi in zip(ell, g, strict=True))),
    )


def mean_quotient(a, b, da, db):
    a, b, da, db = map(interval, (a, b, da, db))
    require(abs(a).a > 0, "strictly positive absolute mean denominator")
    return -b / a, -db / a + b * da / (a * a)


def path_quotient(amin, bmax, damax, dbmax, ea, eb, eda, edb):
    amin, bmax, damax, dbmax, ea, eb, eda, edb = map(interval, (amin, bmax, damax, dbmax, ea, eb, eda, edb))
    require(interval(amin).a > 0, "strictly positive pathwise denominator")
    value = eb / amin + bmax * ea / amin**2
    gradient = edb / amin + dbmax * ea / amin**2
    gradient += (eb * damax + bmax * eda) / amin**2 + 2 * bmax * damax * ea / amin**3
    return upper(value), upper(gradient)


def expectation_quotient(a, b, da, db, ea, eb, eda, edb):
    a, b, da, db, ea, eb, eda, edb = map(interval, (a, b, da, db, ea, eb, eda, edb))
    amin = lower(abs(a) - ea)
    require(amin.a > 0, "strictly positive expectation denominator")
    value = (eb + magnitude(b / a) * ea) / amin
    gradient = edb / amin + magnitude(db) * ea / (abs(a) * amin)
    gradient += (eb * (magnitude(da) + eda) + magnitude(b) * eda) / amin**2
    gradient += magnitude(b * da) * ea * (2 * abs(a) + ea) / (a**2 * amin**2)
    return upper(value), upper(gradient), amin


def budget_row(measure, units, budget, three_se, discrepancy, reference_error, bias, original_pass):
    require((measure, units) in (("Price", "USD"), ("par", "decimal-spread")), "frozen measure units")
    budget, three_se, discrepancy, reference_error, bias = map(
        interval, (budget, three_se, discrepancy, reference_error, bias)
    )
    require(
        budget.a > 0 and min(x.a for x in (three_se, discrepancy, reference_error, bias)) >= 0,
        "positive budget and nonnegative errors",
    )
    precision = three_se + reference_error + bias
    combined = discrepancy + precision
    return dict(
        measure=measure,
        units=units,
        original_budget=budget,
        three_se=three_se,
        original_discrepancy=discrepancy,
        reference_error=reference_error,
        new_systematic_bias=bias,
        new_precision=precision,
        new_combined=combined,
        original_pass=original_pass,
        precision_pass=bool(precision.b <= budget.a and original_pass),
        combined_pass=bool(combined.b <= budget.a and original_pass),
    )


def admit_claim(claim):
    fields = (
        "financial_bias_bound_qualified",
        "actual_joint_law_qualified",
        "caller_binding_qualified",
        "original_numerical_pass",
    )
    require(
        all(claim.get(field) is False for field in fields),
        "unproved actual/global qualification or changed original RED",
    )


def group_totals(terms, name):
    selected = [row[name] for row in terms]
    return {key: sum_iv(row[key] for row in selected) for key in selected[0]}


def coefficient_certificate(terms):
    groups = {name: [] for name in ("A", "B")}
    for term in terms:
        args = term_arguments(term)
        groups[term[0]].append(dict(finite=finite_term(*args), law=law_term(*args)))
    return {
        name: dict(finite=group_totals(rows, "finite"), law=group_totals(rows, "law")) for name, rows in groups.items()
    }


def derived_bounds(groups):
    a, b = groups["A"], groups["B"]
    f, g, x, y = a["finite"], b["finite"], a["law"], b["law"]
    path = path_quotient(
        f["lower_abs"],
        g["max_value"],
        f["max_gradient"],
        g["max_gradient"],
        f["value"],
        g["value"],
        f["gradient"],
        g["gradient"],
    )
    law = expectation_quotient(
        x["mean"],
        y["mean"],
        x["mean_gradient"],
        y["mean_gradient"],
        x["value"],
        y["value"],
        x["gradient"],
        y["gradient"],
    )
    center = mean_quotient(x["mean"], y["mean"], x["mean_gradient"], y["mean_gradient"])
    return dict(
        D1=dict(
            A=f["value"],
            B=g["value"],
            Price=g["value"] + interval(COUPON) * f["value"],
            par=path[0],
            dA=f["gradient"],
            dB=g["gradient"],
            dPrice=g["gradient"] + interval(COUPON) * f["gradient"],
            dpar=path[1],
            absolute_annuity_lower=f["lower_abs"],
        ),
        D2=dict(
            A=x["value"],
            B=y["value"],
            Price=y["value"] + interval(COUPON) * x["value"],
            par=law[0],
            dA=x["gradient"],
            dB=y["gradient"],
            dPrice=y["gradient"] + interval(COUPON) * x["gradient"],
            dpar=law[1],
            absolute_annuity_mean_lower=law[2],
        ),
        centers=dict(
            A=x["mean"],
            B=y["mean"],
            Price=y["mean"] + interval(COUPON) * x["mean"],
            par=center[0],
            dA=x["mean_gradient"],
            dB=y["mean_gradient"],
            dPrice=y["mean_gradient"] + interval(COUPON) * x["mean_gradient"],
            dpar=center[1],
        ),
    )


def native_unit(label):
    if label.startswith("fxspot:"):
        return "USD/EUR"
    if label.startswith("logdf:"):
        return "Dimensionless"
    return "year^-1/2" if label.startswith("sigma:EUR/USD:") else "year^-3/2"


def compact_interval(value):
    return [[int(x) for x in endpoint] for endpoint in interval(value)._mpi_]


def compact_term(term):
    group, c, exponent = term
    components = (c, exponent.k, *exponent.ell)
    return dict(
        group=group,
        values=[compact_interval(x.value) for x in components],
        derivatives={str(i): compact_interval(x.derivative) for i, x in enumerate(components) if x.derivative != 0},
    )


def column_certificate(case, label):
    terms = IndependentTerms(case, label, active=bool(label)).terms()
    require(all(c.value.b < 0 for group, c, _ in terms if group == "A"), "same negative annuity sign")
    result = derived_bounds(coefficient_certificate(terms))
    result["term_count"] = len(terms)
    if label:
        result.update(label=label, parameter_unit=native_unit(label))
    return result, [compact_term(term) for term in terms]


def closure_sanity(case, baseline, native):
    values = [
        magnitude(baseline["centers"][name] - interval(value))
        for name, value in zip(("A", "B", "Price", "par"), case["fast"], strict=True)
    ]
    derivatives = []
    for j, column in enumerate(native):
        derivatives.extend(
            magnitude(column["centers"]["d" + name] - interval(case["fast_gradient"][i][j]))
            for i, name in enumerate(("A", "B", "Price", "par"))
        )
    require(max(x.b for x in values) < interval("1e-10").a, "independent signed cashflow center sanity")
    require(max(x.b for x in derivatives) < interval("1e-10").a, "independent full native derivative sanity")
    return dict(
        value_max=max(values, key=lambda x: x.b),
        native_max=max(derivatives, key=lambda x: x.b),
        purpose="captured fast is only a sanity comparison, never the expected cashflow",
    )


def qualify_case(case):
    admit_case(case)
    baseline, terms = column_certificate(case, "")
    native, derivative_terms = [], {}
    for label in case["labels"]:
        result, details = column_certificate(case, label)
        native.append(result)
        derivative_terms[label] = [row["derivatives"] for row in details]
    identity = {key: case[key] for key in ("fit", "id", "dimension", "source", "instrument", "mapping")}
    return dict(
        identity,
        model=case["model"]["identity"],
        baseline=baseline,
        native=native,
        sanity=closure_sanity(case, baseline, native),
        affine_terms=terms,
        affine_native_derivatives=derivative_terms,
    )


def load_fixture(path):
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == FIXTURE_SHA256, "frozen input identity")
    data = json.loads(raw)
    require(
        [(row["fit"], row["dimension"]) for row in data["cases"]]
        == [(fit, dimension) for fit in ("EXACT", "APPROXIMATE") for dimension in (30, 55, 80)],
        "six ordered frozen snapshots",
    )
    return data["cases"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--fixture", type=Path, default=Path(__file__).with_name("fixtures") / "xccy_bias_frozen_inputs.json"
    )
    parser.add_argument("--budget-report", type=Path, required=True)
    parser.add_argument(
        "--verify", action="store_true", help="recompute and compare every canonical endpoint without writing"
    )
    args = parser.parse_args()
    mp.iv.dps = 70
    cases = load_fixture(args.fixture)
    read_report(args.budget_report)
    profiles = []
    for case in cases:
        profiles.append(qualify_case(case))
        print(case["fit"], case["id"], "D1/D2 finite and conditional bounds generated", flush=True)
    claim = dict(
        financial_bias_bound_qualified=False,
        actual_joint_law_qualified=False,
        caller_binding_qualified=False,
        original_numerical_pass=False,
    )
    admit_claim(claim)
    result = dict(
        schema="dal.xccy-financial-bias-qualification/1",
        **claim,
        domain=normal_domain(),
        precision_bits=mp.iv.prec,
        fixture_sha256=FIXTURE_SHA256,
        conditional_law="independent uniform numerators k=1..4294967087; u=RN(k/4294967088)",
        D1_scope="same frozen uniform vectors; mathematical cashflow transformation perturbation, no IID needed",
        D2_scope="conditional expectation and quotient of coefficient expectations, not expected pathwise ratio",
        D3=combine_report(args.budget_report, profiles, budget_row),
        profiles=profiles,
    )
    canonical = serialize(result)
    if args.verify:
        require(
            json.loads(args.output.read_text()) == canonical,
            "certificate replay: bounds, units, identity or preserved original flags changed",
        )
        print("Complete canonical financial certificate replay passed", flush=True)
    else:
        args.output.write_text(json.dumps(canonical, indent=2) + "\n")


if __name__ == "__main__":
    main()

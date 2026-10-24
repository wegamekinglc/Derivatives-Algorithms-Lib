"""Uniform finite-stream transform component, without any joint-law premise.

The old D1 certificate is read by exact identity, never overwritten. This
program constructs new signed/common-coordinate bounds and does no MC.
"""

import argparse
import hashlib
import json
from fractions import Fraction
from pathlib import Path

import mpmath as mp

from xccy_acceptance_bias_budget import parameter_step
from xccy_acceptance_bias_cashflows import IndependentTerms, admit_case, term_arguments
from xccy_acceptance_bias_intervals import interval, magnitude, require, serialize, upper
from xccy_acceptance_bias_qualification import (
    COUPON,
    FIXTURE_SHA256,
    SCALAR_ERROR,
    SUPPORT,
    admit_claim,
    load_fixture,
    native_unit,
    normal_domain,
)
from xccy_acceptance_finite_math import (
    PolynomialTerm,
    annuity_controls,
    quotient_bounds,
    signed_enclosure,
    spatial_bound,
)

OLD_CERTIFICATE_SHA256 = "2f6666deed5b704edfc37014f99a8ac06e7bb537edd4bbc12754f480cc59ef49"
FLAGS = (
    "financial_bias_bound_qualified",
    "actual_joint_law_qualified",
    "caller_binding_qualified",
    "original_numerical_pass",
)


def frozen_domain(error=SCALAR_ERROR, support=SUPPORT):
    require(Fraction(error) == Fraction("9.006864425e-9"), "unchanged accepted scalar E")
    require(Fraction(support) == Fraction(25, 4), "unchanged rounded-grid support")
    require(mp.__version__ == "1.3.0" and mp.iv.prec == 236, "trusted interval version/precision")
    return dict(normal_domain(), homotopy_radius=interval(support) + interval(error))


def read_accepted(path):
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == OLD_CERTIFICATE_SHA256, "accepted D1 certificate identity")
    result = json.loads(raw)
    admit_claim(result)
    require(
        result["fixture_sha256"] == FIXTURE_SHA256 and result["precision_bits"] == 236,
        "accepted input/precision identity",
    )
    return result


def decode(value):
    bounds = tuple(tuple(value[key]) for key in ("binary_lower", "binary_upper"))
    return mp.iv.make_mpf(bounds)


def polynomial_terms(terms, group, native=False, sign=1):
    result = []
    for term in terms:
        if term[0] != group:
            continue
        c, k, ell, dc, dk, g = term_arguments(term)
        constant = dc + c * dk if native else c
        linear = tuple(c * x if native else interval(0) for x in g)
        result.append(PolynomialTerm(k, ell, sign * constant, tuple(sign * x for x in linear)))
    return result


def relative_controls(b, bt, anchor, radius):
    return dict(
        R=magnitude(signed_enclosure(b, anchor, radius)),
        Rt=magnitude(signed_enclosure(bt, anchor, radius)),
        V=spatial_bound(b, anchor, radius),
        Vt=spatial_bound(bt, anchor, radius),
    )


def finite_column(terms, radius, error):
    d = polynomial_terms(terms, "A", sign=-1)
    dt = polynomial_terms(terms, "A", native=True, sign=-1)
    b = polynomial_terms(terms, "B")
    bt = polynomial_terms(terms, "B", native=True)
    denominator = annuity_controls(d, dt, radius)
    relative = relative_controls(b, bt, denominator["anchor"], radius)
    quotient = quotient_bounds(relative, denominator, error)
    zero = interval(0), tuple(interval(0) for _ in denominator["anchor"][1])
    av, bv, at, btv = (upper(interval(error) * spatial_bound(ts, zero, radius)) for ts in (d, b, dt, bt))
    return dict(
        bounds=dict(
            A=av,
            B=bv,
            Price=upper(bv + interval(COUPON) * av),
            par=quotient["value"],
            dA=at,
            dB=btv,
            dPrice=upper(btv + interval(COUPON) * at),
            dpar=quotient["native"],
        ),
        denominator=denominator,
        relative=relative,
        quotient_native_pieces=quotient["native_pieces"],
    )


def comparison_row(measure, label, old, raw):
    require(old.a >= 0 and raw.a >= 0, "nonnegative transform majorants")
    selected = old if old.b <= raw.b else raw
    base_units = "decimal-spread" if measure == "par" else "USD"
    units = base_units + "/(" + native_unit(label) + ")" if label else base_units
    result = dict(
        measure=measure,
        axis=label or "value",
        units=units,
        old_D1=old,
        new_raw=raw,
        selected=selected,
        improved=bool(raw.b < old.a),
    )
    if measure in ("Price", "par"):
        budget = interval(".0025" if measure == "Price" else ".000025") / interval(parameter_step(label))
        result.update(original_budget=budget, component_bound_within_budget=bool(selected.b <= budget.a))
    else:
        result.update(original_budget=None, component_bound_within_budget=None)
    return result


def qualify_profile(case, inherited, radius, error):
    admit_case(case)
    identity = {key: case[key] for key in ("fit", "id", "dimension", "source", "instrument", "mapping")}
    require(all(inherited[key] == value for key, value in identity.items()), "six-profile identity")
    require(inherited["model"] == case["model"]["identity"], "model identity")
    require([row["label"] for row in inherited["native"]] == case["labels"], "eleven native identities")
    columns, rows = [], []
    old_columns = [inherited["baseline"], *inherited["native"]]
    for label, old in zip(["", *case["labels"]], old_columns, strict=True):
        terms = IndependentTerms(case, label, active=bool(label)).terms()
        column = finite_column(terms, radius, error)
        columns.append(dict(axis=label or "value", **column))
        for measure in ("A", "B", "Price", "par"):
            key = ("d" if label else "") + measure
            rows.append(comparison_row(measure, label, decode(old["D1"][key]), column["bounds"][key]))
    return dict(identity, model=case["model"]["identity"], columns=columns, comparisons=rows)


def certificate(fixture, accepted):
    domain = frozen_domain()
    cases, inherited = load_fixture(fixture), read_accepted(accepted)
    profiles = []
    for case, old in zip(cases, inherited["profiles"], strict=True):
        profiles.append(qualify_profile(case, old, domain["homotopy_radius"], interval(SCALAR_ERROR)))
        print(case["fit"], case["dimension"], "signed finite-stream certificate generated", flush=True)
    return dict(
        schema="dal.xccy-finite-stream-bias/1",
        **{name: False for name in FLAGS},
        scope="mathematical transform component, conditional on scalar caller applicability; zero new MC",
        mean_premise=(
            "arbitrary identical nonnegative normalized weights, shared vector for every cashflow/native"
        ),
        domain=domain,
        precision_bits=mp.iv.prec,
        fixture_sha256=FIXTURE_SHA256,
        accepted_D1_sha256=OLD_CERTIFICATE_SHA256,
        inherited_D2_D3="unchanged; not recomputed and not promoted by this finite-stream theorem",
        profiles=profiles,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--accepted", type=Path, required=True)
    parser.add_argument(
        "--fixture", type=Path, default=Path(__file__).with_name("fixtures") / "xccy_bias_frozen_inputs.json"
    )
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    mp.iv.dps = 70
    result = serialize(certificate(args.fixture, args.accepted))
    if args.verify:
        require(
            json.loads(args.output.read_text()) == result,
            "complete canonical finite-stream certificate replay",
        )
        print("Complete canonical finite-stream replay passed")
    else:
        args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()

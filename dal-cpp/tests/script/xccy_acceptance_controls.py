"""Independent kernel, foreign-measure and outstanding-cashflow controls."""

import argparse
import json
from pathlib import Path

import mpmath as mp

from xccy_acceptance_reference import (
    BrownianCashflows,
    EPOCH,
    END,
    FIX1,
    PAYMENT,
    START,
    kernel_integral,
    number,
    require,
)


def kernel_refinements(row):
    refinements = []
    for digits, panels in ((45, 1), (65, 2), (85, 4)):
        with mp.workdps(digits):
            env = BrownianCashflows(row, digits, panels)
            refinements.append([[env.covariance(i, 365, j, 365) for j in range(5)] for i in range(5)])
    return refinements


def kernel_entry(row, refinements, i, j):
    expected = refinements[-1]
    scale = mp.sqrt(expected[i][i] * expected[j][j])
    actual = number(row["covariance"][i][j])
    rebuilt = mp.fsum(number(row["lower"][i][k]) * number(row["lower"][j][k]) for k in range(5))
    if not scale:
        require(actual == rebuilt == expected[i][j] == 0, "exact kernel-zero rows")
        return None
    indicator = (
        4
        * max(
            abs(refinements[1][i][j] - refinements[0][i][j]),
            abs(refinements[2][i][j] - refinements[1][i][j]),
        )
        / scale
    )
    return (
        abs(actual - expected[i][j]) / scale,
        abs(rebuilt - expected[i][j]) / scale,
        indicator,
    )


def pivot_mutant(row, expected):
    if row["scenario"] != "tiny":
        return None
    require(
        row["lower"][0][0] > 0 and row["covariance"][0][4] != 0,
        "positive pivot and xd/Z cross-term survive",
    )
    error = abs(expected[0][4]) / mp.sqrt(expected[0][0] * expected[4][4])
    require(
        error > mp.mpf("1e-12"),
        "global-pivot truncation mutant fails normalized reconstruction",
    )
    return mp.nstr(error, 20)


def kernel_control(row):
    refinements = kernel_refinements(row)
    with mp.workdps(95):
        entries = [kernel_entry(row, refinements, i, j) for i in range(5) for j in range(5)]
        columns = list(zip(*(entry for entry in entries if entry is not None))) or [
            (),
            (),
            (),
        ]
        normalized, reconstruction, indicators = (max(column, default=0) for column in columns)
        require(
            normalized <= mp.mpf("1e-12") and reconstruction <= mp.mpf("1e-12"),
            "independent normalized kernel reconstruction",
        )
        return {
            "scenario": row["scenario"],
            "max_covariance_error": mp.nstr(normalized, 20),
            "max_reconstruction_error": mp.nstr(reconstruction, 20),
            "max_refinement_indicator": mp.nstr(indicators, 20),
            "pivot_mutant_error": pivot_mutant(row, refinements[-1]),
        }


def kernel_controls(rows):
    return [kernel_control(row) for row in rows]


def qf_mean(env, slot, date):
    if slot in (1, 3):
        return mp.mpf(0)
    if slot == 4:
        return env.covariance(4, date, 4, date)
    result, previous = mp.mpf(0), 0
    for bucket, boundary in enumerate(env.ends):
        hi = min(date, boundary)
        if hi > previous:
            result += (
                env.r[0][2]
                * env.sigma[0][bucket]
                * env.sigma[2][bucket]
                * kernel_integral(
                    env.a,
                    slot,
                    date,
                    4,
                    date,
                    previous,
                    hi,
                    env.digits,
                    env.subdivisions,
                    False,
                )
            )
        previous = boundary
    return result


def qf_expectation(env, exponent):
    constant, loadings = exponent
    linear = mp.fsum(w * qf_mean(env, slot, date) for (slot, date), w in loadings.items())
    variance = mp.fsum(w * z * env.covariance(i, ti, j, tj) for (i, ti), w in loadings.items() for (j, tj), z in loadings.items())
    return mp.exp(constant + linear + variance / 2)


def measure_controls(cases):
    results = []
    for scenario in ("flat", "nonflat"):
        case = next(x for x in cases if x["id"] == f"{scenario}/FIXED/domestic/receive")
        with mp.workdps(85):
            env = BrownianCashflows(case, 85, 4)
            for payment in (365, 730):
                domestic = env.expectation(env.discount(payment))
                foreign = env.expectation(env.product(env.discount(payment), env.fx(payment)))
                require(
                    abs(domestic - mp.exp(env.effective(False, payment))) < mp.mpf("1e-70"),
                    "domestic discounted unit martingale",
                )
                require(
                    abs(foreign - env.spot * mp.exp(env.effective(True, payment))) < mp.mpf("1e-70"),
                    "Pf/B foreign unit martingale",
                )
            fix, start, end, pay = ((x - EPOCH).days for x in (FIX1, START, END, PAYMENT))
            growth = env.growth(True, fix, start, end)
            domestic = env.expectation(env.product(growth, env.fx(pay), env.discount(pay))) - env.expectation(
                env.product(env.fx(pay), env.discount(pay))
            )
            foreign_bank = -env.phi(True, pay), {(3, pay): -mp.mpf(1)}
            foreign = env.spot * (qf_expectation(env, env.product(growth, foreign_bank)) - qf_expectation(env, foreign_bank))
            require(
                abs(domestic - foreign) < mp.mpf("1e-70"),
                "coupon Qd versus independent Qf drift proof",
            )
            results.append(
                {
                    "scenario": scenario,
                    "foreign_coupon_qd": mp.nstr(100 * domestic, 88),
                    "foreign_coupon_qf": mp.nstr(100 * foreign, 88),
                    "difference": mp.nstr(100 * (domestic - foreign), 12),
                }
            )
    return results


def history_refinements(row):
    refinements, wide = [], []
    for digits, panels in ((45, 1), (65, 2), (85, 4)):
        with mp.workdps(digits):
            refinements.append(BrownianCashflows(row, digits, panels).financial())
    for digits in (50, 80):
        with mp.workdps(digits):
            wide.append(BrownianCashflows(row, digits, closed=True).replay())
    return refinements, wide


def history_quantity_pass(error, eo, ef):
    return eo <= mp.mpf("2.5e-11") and ef <= mp.mpf("2.5e-11") and error <= eo + ef + mp.mpf("1e-11") <= mp.mpf("1e-10")


def history_indicators(row, refinements, wide):
    indicators = [4 * max(abs(number(actual) - wide[0][i]), abs(wide[0][i] - wide[1][i])) for i, actual in enumerate(row["fast"])]
    e_oracle = [
        4
        * max(
            abs(refinements[0][i] - refinements[1][i]),
            abs(refinements[1][i] - refinements[2][i]),
        )
        for i in range(3)
    ]
    return indicators, e_oracle


def history_control(row):
    refinements, wide = history_refinements(row)
    with mp.workdps(95):
        values = refinements[-1]
        errors = [abs(number(actual) - expected) for actual, expected in zip(row["fast"], values)]
        require(max(errors) <= mp.mpf("1e-10"), "independent outstanding cashflow table")
        require(row["par_refused"] == (values[0] == 0), "settled spread-leg annuity refusal")
        if row["model"]["valuation"] >= "2027-11-05":
            require(row["legacy_refused"], "legacy outstanding-domain refusal retained")
    with mp.workdps(95):
        indicators, e_oracle = history_indicators(row, refinements, wide)
        require(
            all(history_quantity_pass(error, eo, ef) for error, eo, ef in zip(errors, e_oracle, indicators)),
            "history independent and fast-numeric direct gate",
        )
        return {
            "valuation": row["model"]["valuation"],
            "foreign_spread": row["foreign_spread"],
            "independent": [None if x is None else mp.nstr(x, 88) for x in values],
            "max_error": mp.nstr(max(errors), 20),
            "par_refused": row["par_refused"],
            "legacy_refused": row["legacy_refused"],
            "fast_indicators": list(map(str, indicators)),
            "oracle_indicators": list(map(str, e_oracle)),
            "qualification": "direct A/B/Price gate with separate oracle and wide lowered-term indicators",
        }


def history_controls(rows):
    return [history_control(row) for row in rows]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path)
    parser.add_argument("controls", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.controls.read_text())
    output = {
        "kernels": kernel_controls(data["kernels"]),
        "foreign_measure": measure_controls(json.loads(args.captures.read_text())),
        "history": history_controls(data["history"]),
    }
    args.output.write_text(json.dumps(output, indent=2) + "\n")
    print("PASS: five kernel/rank cases, two Qf coupon identities, six outstanding-cashflow cases")


if __name__ == "__main__":
    main()

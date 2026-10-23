"""Independent extended calendar/cashflows, staged quotes and historical risk.

Geometry is checked before deriving quotes; staged capture consumes only those
quotes. Neither independent financial calculation reads production moment terms.
"""

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path

import mpmath as mp

from xccy_acceptance_reference import BrownianCashflows, EPOCH, POLICY, day, gate, number, require

ENDS = tuple(dt.date.fromisoformat(s) for s in ("2027-05-04", "2027-08-04", "2027-11-04", "2028-02-04", "2028-05-04", "2028-08-04", "2028-11-04"))
MATURITIES = ("2027-11-04", "2028-05-04", "2028-11-04")
GRADIENT_CASES = {
    "rho-df/FIXED/foreign/pay",
    "rho-dx/RESETTABLE/domestic/receive",
    "rho-fx/MARK_TO_MARKET/foreign/pay",
    "negative-basis/RESETTABLE/foreign/receive",
    "zero-basis/MARK_TO_MARKET/domestic/pay",
    "own-projection/FIXED/domestic/receive",
    "nonflat/MARK_TO_MARKET/foreign/pay",
}


def business_shift(date, count):
    direction = 1 if count >= 0 else -1
    remaining = abs(count)
    while remaining:
        date += dt.timedelta(days=direction)
        if date.weekday() < 5:
            remaining -= 1
    return date


def independent_period(start, end, foreign, lag):
    return {
        "start": start.isoformat(),
        "end": end.isoformat(),
        "payment": business_shift(end, lag if foreign else 0).isoformat(),
        "fixing": business_shift(start, -2).isoformat(),
        "index": "IR:EUR,LIBOR_6M_LCH" if foreign else "IR:USD,LIBOR_3M_LCH",
        "alpha": (end - start).days / 360.0,
    }


def independent_schedule(case):
    maturity = case.get("maturity", "2027-11-04")
    require(maturity in MATURITIES, "frozen full-canonical maturity")
    last = MATURITIES.index(maturity) * 2 + 2
    dates = ENDS[: last + 1]
    lag = case.get("payment_lag", 2)
    domestic = [independent_period(a, b, False, lag) for a, b in zip(dates, dates[1:])]
    foreign = [independent_period(dates[i], dates[i + 2], True, lag) for i in range(0, last, 2)]
    return domestic, foreign


def qualify_schedule(case):
    domestic, foreign = independent_schedule(case)
    require(case["domestic_periods"] == domestic, "independent extended USD calendar/index table")
    require(case["foreign_periods"] == foreign, "independent extended EUR calendar/index table")
    basis = next(c for c in case["model"]["curves"] if c["route"] == "BASIS:EUR/USD")
    require(max(p["payment"] for p in domestic + foreign) <= basis["dates"][-1], "actual basis payment coverage")
    return domestic, foreign


class ExtendedCashflows(BrownianCashflows):
    def known(self, index, fixing):
        if fixing > self.valuation:
            return None
        key = (EPOCH + dt.timedelta(days=fixing)).isoformat()
        value = self.case.get("history", {}).get(index, {}).get(key)
        require(fixing >= self.valuation or value is not None, "independent strict-past fixing dependency")
        return value

    def reset(self, date):
        known = self.known("FX[EUR/USD]", date)
        return (mp.log(number(known)), {}) if known is not None else self.fx(date)

    def notional(self, period, dates):
        if period == 0 or self.case["mode"] == "FIXED":
            return mp.log(number(110.0)), {}
        return self.product((mp.log(number(110.0) * number(100.0 / 110.0)), {}), self.reset(day(dates[period]["start"])))

    def coupon_growth_at(self, period, foreign):
        fixing, start, end = (day(period[key]) for key in ("fixing", "start", "end"))
        known = self.known(period["index"], fixing)
        if known is not None:
            return mp.log1p(number(period["alpha"]) * number(known)), {}
        return self.growth(foreign, fixing, start, end)

    def coupon_value(self, period, foreign, notional, sign):
        payment = day(period["payment"])
        base = self.product(notional, self.discount(payment), self.fx(payment) if foreign else (mp.mpf(0), {}))
        growth = self.coupon_growth_at(period, foreign)
        weight = sign * (-1 if foreign else 1)
        b = weight * (self.expectation(self.product(base, growth)) - self.expectation(base))
        a = weight * number(period["alpha"]) * self.expectation(base) if foreign == self.case["foreign_spread"] else mp.mpf(0)
        return a, b

    def coupons(self, domestic, foreign, sign):
        a, b = mp.mpf(0), mp.mpf(0)
        for period, row in enumerate(domestic):
            if day(row["payment"]) >= self.valuation:
                da, db = self.coupon_value(row, False, self.notional(period, domestic), sign)
                a, b = a + da, b + db
        foreign_n = mp.log(number(110.0) * number(100.0 / 110.0)), {}
        for row in foreign:
            if day(row["payment"]) >= self.valuation:
                da, db = self.coupon_value(row, True, foreign_n, sign)
                a, b = a + da, b + db
        return a, b

    def principal(self, domestic, sign):
        start, end = day(domestic[0]["start"]), day(domestic[-1]["end"])
        foreign_n = number(110.0) * number(100.0 / 110.0)
        b = mp.mpf(0)
        if start >= self.valuation:
            b += sign * (
                -number(110.0) * self.expectation(self.discount(start))
                + foreign_n * self.expectation(self.product(self.fx(start), self.discount(start)))
            )
        if end >= self.valuation:
            b += sign * (
                self.expectation(self.product(self.notional(len(domestic) - 1, domestic), self.discount(end)))
                - foreign_n * self.expectation(self.product(self.fx(end), self.discount(end)))
            )
        return b

    def mtm(self, domestic, sign):
        if self.case["mode"] != "MARK_TO_MARKET":
            return mp.mpf(0)
        b = mp.mpf(0)
        for period in range(1, len(domestic)):
            date = day(domestic[period]["start"])
            if date >= self.valuation:
                next_value = self.expectation(self.product(self.notional(period, domestic), self.discount(date)))
                prior_value = self.expectation(self.product(self.notional(period - 1, domestic), self.discount(date)))
                b += sign * (next_value - prior_value)
        return b

    def financial(self):
        domestic, foreign = independent_schedule(self.case)
        sign = (1 if self.case["foreign_spread"] else -1) * (1 if self.case["receive"] else -1)
        a, b = self.coupons(domestic, foreign, sign)
        b += self.principal(domestic, sign)
        b += self.mtm(domestic, sign)
        return self.coefficient_values(a, b)


def refined_values(case):
    values, wide = [], []
    for digits, panels in zip(POLICY["digits"], POLICY["subdivisions"]):
        with mp.workdps(digits):
            values.append(ExtendedCashflows(case, digits, panels).financial())
    for digits in POLICY["wide_digits"]:
        with mp.workdps(digits):
            wide.append(ExtendedCashflows(case, digits, closed=True).replay())
    return values, wide


def finite_difference(case, label, step, digits, panels, closed, size):
    def fn(bump):
        env = ExtendedCashflows(case, digits, panels, closed, label, bump)
        return env.replay() if closed else env.financial()

    if closed:
        return [mp.diff(lambda x: fn(x)[i], mp.mpf(0)) for i in range(size)]
    samples = [fn(multiplier * step) for multiplier in (2, 1, -1, -2)]
    return [(-samples[0][i] + 8 * samples[1][i] - 8 * samples[2][i] + samples[3][i]) / (12 * step) for i in range(size)]


def gradient_rows(case, label, step, size):
    fd, ad = [], []
    for digits, panels in zip(POLICY["digits"], POLICY["subdivisions"]):
        with mp.workdps(digits):
            fd.append([finite_difference(case, label, step / divisor, digits, panels, False, size) for divisor in (1, 2, 4)])
    for digits in POLICY["wide_digits"]:
        with mp.workdps(digits):
            ad.append(finite_difference(case, label, step, digits, 1, True, size))
    return fd, ad


def successive_error(rows, column):
    return 4 * max(abs(rows[1][column] - rows[0][column]), abs(rows[2][column] - rows[1][column]))


def serialize_fd(rows):
    return [[[mp.nstr(x, 88) for x in row] for row in refinement] for refinement in rows]


def gradient_gate(case, column, label, size):
    scale = mp.mpf(".01") if label.startswith(("fxspot:", "sigma:EUR/USD:")) else mp.mpf(".001")
    step = scale * mp.mpf(".001")
    fd, ad = gradient_rows(case, label, step, size)
    with mp.workdps(95):
        error = [successive_error(fd[-1], i) + successive_error([row[-1] for row in fd], i) for i in range(size)]
        values = [
            gate(
                case["fast_gradient"][i][column],
                fd[-1][-1][i],
                error[i],
                [row[i] for row in ad],
                number(POLICY["epsilon_det"][i]) / scale,
                number(POLICY["deterministic_budget"][i]) / scale,
            )
            for i in range(size)
        ]
    return {
        "label": label,
        "scale": str(scale),
        "h": str(step),
        "values": values,
        "fd": serialize_fd(fd),
    }


def selected_gradients(case):
    if case["scenario"] == "staged":
        return [label for label in case["labels"] if label.startswith("logdf:USD:BASIS:")]
    if case["scenario"] == "history" or case["id"] in GRADIENT_CASES:
        return case["labels"]
    return []


def check_case(case):
    qualify_schedule(case)
    values, wide = refined_values(case)
    size = len(case["fast_gradient"])
    with mp.workdps(95):
        gates = [
            gate(
                case["fast"][i],
                values[-1][i],
                successive_error(values, i),
                [row[i] for row in wide],
                number(POLICY["epsilon_det"][i]),
                number(POLICY["deterministic_budget"][i]),
            )
            for i in range(len(case["fast"]))
        ]
    selected = selected_gradients(case)
    gradients = [gradient_gate(case, column, label, size) for column, label in enumerate(case["labels"]) if label in selected]
    return {"id": case["id"], "values": gates, "gradients": gradients}


def independent_quote(case):
    qualify_schedule(case)
    values = []
    for digits, panels in zip(POLICY["digits"], POLICY["subdivisions"]):
        with mp.workdps(digits):
            values.append(ExtendedCashflows(case, digits, panels).financial())
    with mp.workdps(95):
        errors = [successive_error(values, i) for i in range(4)]
        require(all(e <= number(t) / 4 for e, t in zip(errors, POLICY["deterministic_budget"])), "independent staged quote precision")
    return {
        "id": case["id"],
        "values": [mp.nstr(x, 88) for x in values[-1]],
        "refinements": [[mp.nstr(x, 88) for x in row] for row in values],
        "oracle_indicators": list(map(str, errors)),
    }


def derive_quotes(input_path, output_path, quote_path):
    geometry = json.loads(input_path.read_text())
    require(geometry["knots"] == ["2027-11-08", "2028-05-08", "2028-11-08"], "three frozen free basis nodes")
    rows = [independent_quote(case) for case in geometry["cases"]]
    output_path.write_text(json.dumps({"geometry_sha256": hashlib.sha256(input_path.read_bytes()).hexdigest(), "quotes": rows}, indent=2) + "\n")
    quote_path.write_text("\n".join(format(float(row["values"][3]), ".17g") for row in rows) + "\n")


def staged_norms(fit, quotes):
    residuals = [(measure - quote) / 1e-4 for measure, quote in zip(fit["model_measures"], quotes)]
    require(fit["row_scales"] == [1e-4] * 3, "frozen native row scales")
    require(fit["row_ids"] == ["independent-staged-" + str(i) for i in range(3)], "same original observation row axes")
    require(abs(max(map(abs, residuals)) - fit["max_abs_scaled"]) <= 1e-10, "reported original scaled norm")
    norm = sum(x * x for x in residuals) ** 0.5
    require(abs(norm - fit["scaled_norm"]) <= 1e-10, "independent original scaled L2")
    require(fit["fit_within_tolerance"], "actual fit flag")
    require(max(map(abs, residuals)) <= 1 if fit["mode"] == "EXACT" else norm <= 0.5, "original EXACT/APPROXIMATE norms")
    return residuals


def staged_jacobian(fit, cases):
    dates = ["2027-11-08", "2028-05-08", "2028-11-08"]
    require([p["date"] for p in fit["free_parameters"]] == dates, "three native free basis node axes")
    require(len(set(p["component"] for p in fit["free_parameters"])) == 1, "single log-discount component")
    determinant = mp.det(mp.matrix(fit["raw_jacobian"]))
    require(abs(determinant) > mp.mpf("1e-4"), "three free-node raw Jacobian rank")
    for row, case in enumerate(cases):
        require(abs(float(case["values"][3]["oracle"]) - fit["model_measures"][row]) <= 1e-12, "independent recovered par")
        columns = {g["label"]: g["values"][3] for g in case["gradients"]}
        for column, date in enumerate(dates):
            value = columns["logdf:USD:BASIS:EUR/USD:" + date]
            require(abs(fit["raw_jacobian"][row][column] - float(value["oracle"])) <= 1e-9, "independent native raw basis Jacobian")
    return determinant


def staged_fit(fit, quotes):
    cases = [check_case(case) for case in fit["cases"]]
    residuals = staged_norms(fit, quotes)
    determinant = staged_jacobian(fit, cases)
    basket = fit["held_out_basket"]
    require(not basket["admitted"] and not basket["draws_started"], "mixed-D basket refused before drawing")
    require(basket["code"] == "CalibrationContextMismatch" and basket["field"] == "/sampling/mapping", "exact mixed-D refusal")
    return {
        "mode": fit["mode"],
        "parameters": fit["parameters"],
        "scaled_residuals": residuals,
        "scaled_l2": fit["scaled_norm"],
        "rank_determinant": str(determinant),
        "held_out_basket": basket,
        "cases": cases,
    }


def require_gates(cases):
    require(all(value["passed"] for case in cases for value in case["values"]), "extended direct value gate; raw result retained")
    require(
        all(value["passed"] for case in cases for gradient in case["gradients"] for value in gradient["values"]),
        "extended direct gradient gate; raw result retained",
    )


def check_capture(input_path, output_path, staged):
    data = json.loads(input_path.read_text())
    if staged:
        fits = [staged_fit(fit, data["quotes"]) for fit in data["fits"]]
        output = {"fits": fits}
        cases = [case for fit in fits for case in fit["cases"]]
    else:
        cases = [check_case(case) for case in data]
        output = {"cases": cases}
    output["script_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    output["capture_sha256"] = hashlib.sha256(input_path.read_bytes()).hexdigest()
    output_path.write_text(json.dumps(output, indent=2) + "\n")
    require_gates(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("quotes", "staged", "cases"))
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--quote-file", type=Path)
    args = parser.parse_args()
    if args.mode == "quotes":
        require(args.quote_file is not None, "explicit independent quote destination")
        derive_quotes(args.input, args.output, args.quote_file)
    else:
        check_capture(args.input, args.output, args.mode == "staged")
    print("PASS", args.mode)


if __name__ == "__main__":
    main()

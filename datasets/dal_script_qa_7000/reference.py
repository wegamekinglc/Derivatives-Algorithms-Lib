from __future__ import annotations

import calendar
import itertools
import math
from datetime import date, timedelta

SCENARIOS = [
    {"name": "downward_spot_positive_rate", "spot_multiplier": 0.65, "rate": 0.015},
    {"name": "base_spot_negative_rate", "spot_multiplier": 1.0, "rate": -0.01},
    {"name": "upward_spot_positive_rate", "spot_multiplier": 1.45, "rate": 0.04},
]


def add_months(value, months):
    month_number = value.year * 12 + value.month - 1 + months
    year, month = divmod(month_number, 12)
    month += 1
    return date(year, month, min(value.day, calendar.monthrange(year, month)[1]))


def following(value):
    while value.weekday() >= 5:
        value += timedelta(days=1)
    return value


def modified_following(value):
    adjusted = following(value)
    if adjusted.month == value.month:
        return adjusted
    while value.weekday() >= 5:
        value -= timedelta(days=1)
    return value


def libor_zero_vol(fixing, rate):
    start = fixing
    for _ in range(2):
        start = following(start + timedelta(days=1))
    end = add_months(start, 3)
    days = (end - start).days
    return math.expm1(rate * days / 365) / (days / 360)


def basis_30_360(start, end):
    start_day = min(start.day, 30)
    end_day = min(end.day, 30) if start_day == 30 else end.day
    return (
        (end.year - start.year) * 360
        + (end.month - start.month) * 30
        + end_day
        - start_day
    ) / 360


def swap_zero_vol(start, rate):
    maturity = add_months(start, 12)
    unadjusted = [start]
    while add_months(unadjusted[-1], 6) <= maturity:
        unadjusted.append(add_months(unadjusted[-1], 6))
    if unadjusted[-1] != maturity:
        unadjusted.append(maturity)
    endpoints = [modified_following(d) for d in unadjusted]
    annuity = sum(
        (
            basis_30_360(a, b) * math.exp(-rate * (b - start).days / 365)
            for a, b in itertools.pairwise(endpoints)
        )
    )
    floating_pv = math.exp(-rate * (endpoints[0] - start).days / 365) - math.exp(
        -rate * (endpoints[-1] - start).days / 365
    )
    return floating_pv / annuity


class DeterministicPayoff:
    def __init__(self, record, scenario):
        self.family = record["family"]
        self.terms = record["contract_terms"]
        self.market = record["pricing_context"]["model"]
        self.today = date.fromisoformat(record["pricing_context"]["evaluation_date"])
        self.maturity = date.fromisoformat(self.terms["maturity"])
        self.rate = scenario["rate"]
        self.multiplier = scenario["spot_multiplier"]
        self.quantity, self.strike = (self.terms["quantity"], self.terms["strike"])
        self.initial = self.terms["initial_price"]
        self.dividend = self.market.get("div", 0.01)
        self.terminal = self.spot(self.maturity)
        self.call = self.positive(self.terminal - self.strike)
        self.put = self.positive(self.strike - self.terminal)
        self.observation_dates = self.terms.get("observation_dates", [])
        self.prices = [self.spot(d) for d in self.observation_dates]
        self.average = sum(self.prices) / len(self.prices) if self.prices else 0
        self.notional = self.terms.get("notional")
        self.fixed_rate = self.terms.get("fixed_rate")

    def time(self, value):
        value = date.fromisoformat(value) if isinstance(value, str) else value
        return (value - self.today).days / 365

    def discount(self, value):
        return math.exp(-self.rate * self.time(value))

    def spot(self, value):
        return (
            self.initial
            * self.multiplier
            * math.exp((self.rate - self.dividend) * self.time(value))
        )

    def positive(self, value):
        return max(value, 0.0)

    def pv_european_call(self):
        payout = self.call
        return self.quantity * payout * self.discount(self.maturity)

    def pv_european_put(self):
        payout = self.put
        return self.quantity * payout * self.discount(self.maturity)

    def pv_cash_digital_call(self):
        triggered = self.terminal > self.strike
        payout = self.terms["digital_cash"] if triggered else 0
        return self.quantity * payout * self.discount(self.maturity)

    def pv_cash_digital_put(self):
        triggered = self.terminal < self.strike
        payout = self.terms["digital_cash"] if triggered else 0
        return self.quantity * payout * self.discount(self.maturity)

    def pv_asset_digital_call(self):
        triggered = self.terminal > self.strike
        payout = self.terminal if triggered else 0
        return self.quantity * payout * self.discount(self.maturity)

    def pv_asset_digital_put(self):
        triggered = self.terminal < self.strike
        payout = self.terminal if triggered else 0
        return self.quantity * payout * self.discount(self.maturity)

    def pv_forward_long(self):
        payout = self.terminal - self.strike
        return self.quantity * payout * self.discount(self.maturity)

    def pv_forward_short(self):
        payout = self.strike - self.terminal
        return self.quantity * payout * self.discount(self.maturity)

    def pv_bull_call_spread(self):
        payout = self.call - self.positive(
            self.terminal - self.strike - self.terms["width"]
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_bear_put_spread(self):
        payout = (
            self.positive(self.strike + self.terms["width"] - self.terminal) - self.put
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_straddle(self):
        payout = self.call + self.put
        return self.quantity * payout * self.discount(self.maturity)

    def pv_strangle(self):
        payout = self.put + self.positive(
            self.terminal - self.strike - self.terms["width"]
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_butterfly(self):
        width = self.terms["width"]
        payout = (
            self.call
            - 2 * self.positive(self.terminal - self.strike - width)
            + self.positive(self.terminal - self.strike - 2 * width)
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_call_ratio_spread(self):
        payout = self.call - 2 * self.positive(
            self.terminal - self.strike - self.terms["width"]
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_capped_call(self):
        payout = min(self.call, self.terms["cap"])
        return self.quantity * payout * self.discount(self.maturity)

    def pv_interval_cash(self):
        payout = (
            self.terms["digital_cash"]
            if self.terms["lower"] <= self.terminal <= self.terms["upper"]
            else 0
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_tiered_cash(self):
        if self.terminal == self.strike:
            payout = self.terms["digital_cash"]
        elif self.strike < self.terminal < self.strike + self.terms["width"]:
            payout = 2 * self.terms["digital_cash"]
        elif self.terminal >= self.strike + self.terms["width"]:
            payout = 3 * self.terms["digital_cash"]
        else:
            payout = 0
        return self.quantity * payout * self.discount(self.maturity)

    def pv_power_call(self):
        payout = self.positive(
            self.terminal * self.terminal - self.strike * self.strike
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_log_contract(self):
        payout = math.log(self.terminal / self.initial)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_geometric_return_call(self):
        payout = self.positive(
            self.terminal / self.initial - 1 - self.terms["return_strike"]
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_arithmetic_asian_call(self):
        payout = self.positive(self.average - self.strike)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_arithmetic_asian_put(self):
        payout = self.positive(self.strike - self.average)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_geometric_asian_call(self):
        geometric_average = math.prod(self.prices) ** (1 / len(self.prices))
        payout = self.positive(geometric_average - self.strike)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_weighted_asian(self):
        payout = self.positive(
            sum((s * w for s, w in zip(self.prices, self.terms["weights"])))
            / sum(self.terms["weights"])
            - self.strike
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_floating_asian_call(self):
        payout = self.positive(self.terminal - self.average)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_lookback_fixed_call(self):
        payout = self.positive(max(self.prices) - self.strike)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_lookback_fixed_put(self):
        payout = self.positive(self.strike - min(self.prices))
        return self.quantity * payout * self.discount(self.maturity)

    def pv_lookback_float_call(self):
        payout = self.terminal - min(self.prices)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_range_option(self):
        payout = max(self.prices) - min(self.prices)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_cliquet(self):
        consecutive = [self.initial, *self.prices]
        returns = [b / a - 1 for a, b in itertools.pairwise(consecutive)]
        payout = sum(returns)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_capped_cliquet(self):
        consecutive = [self.initial, *self.prices]
        returns = [b / a - 1 for a, b in itertools.pairwise(consecutive)]
        payout = self.positive(
            sum(
                min(max(r, self.terms["local_floor"]), self.terms["local_cap"])
                for r in returns
            )
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_realized_variance(self):
        consecutive = [self.initial, *self.prices]
        [b / a - 1 for a, b in itertools.pairwise(consecutive)]
        payout = (
            sum((math.log(b / a) ** 2 for a, b in itertools.pairwise(consecutive)))
            * self.terms["annualization"]
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_realized_volatility(self):
        consecutive = [self.initial, *self.prices]
        [b / a - 1 for a, b in itertools.pairwise(consecutive)]
        payout = (
            sum((math.log(b / a) ** 2 for a, b in itertools.pairwise(consecutive)))
            * self.terms["annualization"]
        )
        payout = math.sqrt(payout)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_up_out_call(self):
        hit = any(price >= self.terms["upper"] for price in self.prices)
        payout = self.call
        payout *= not hit
        return self.quantity * payout * self.discount(self.maturity)

    def pv_down_out_put(self):
        hit = any(price <= self.terms["lower"] for price in self.prices)
        payout = self.put
        payout *= not hit
        return self.quantity * payout * self.discount(self.maturity)

    def pv_up_in_call(self):
        hit = any(price >= self.terms["upper"] for price in self.prices)
        payout = self.call
        payout *= hit
        return self.quantity * payout * self.discount(self.maturity)

    def pv_down_in_put(self):
        hit = any(price <= self.terms["lower"] for price in self.prices)
        payout = self.put
        payout *= hit
        return self.quantity * payout * self.discount(self.maturity)

    def pv_double_out_call(self):
        hit = any(
            price <= self.terms["lower"] or price >= self.terms["upper"]
            for price in self.prices
        )
        payout = self.call
        payout *= not hit
        return self.quantity * payout * self.discount(self.maturity)

    def pv_double_in_put(self):
        hit = any(
            price <= self.terms["lower"] or price >= self.terms["upper"]
            for price in self.prices
        )
        payout = self.put
        payout *= hit
        return self.quantity * payout * self.discount(self.maturity)

    def pv_autocall(self):
        for d, price in zip(self.observation_dates, self.prices):
            if price >= self.terms["trigger"]:
                return (self.terms["principal"] + self.terms["coupon"]) * self.discount(
                    d
                )
        return self.terms["principal"] * self.discount(self.maturity)

    def pv_memory_coupon(self):
        memory, pv = (0, 0)
        for d, price in zip(self.observation_dates, self.prices):
            memory += self.terms["coupon"]
            if price >= self.terms["trigger"]:
                pv += memory * self.discount(d)
                memory = 0
        return pv + self.terms["principal"] * self.discount(self.maturity)

    def pv_corridor_coupon(self):
        return (
            self.quantity
            * self.terms["coupon"]
            * sum(
                (
                    self.discount(d)
                    for d, price in zip(self.observation_dates, self.prices)
                    if self.terms["lower"] <= price <= self.terms["upper"]
                )
            )
        )

    def pv_equity_linked_note(self):
        principal = (
            self.terms["principal"]
            if self.terminal >= self.terms["trigger"]
            else self.terms["principal"] * self.terminal / self.terms["trigger"]
        )
        return (principal + self.terms["coupon"]) * self.discount(self.maturity)

    def pv_forward_start_call(self):
        payout = self.positive(
            self.terminal
            - self.terms["moneyness"] * self.spot(self.terms["reset_date"])
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_delayed_settlement_call(self):
        return self.quantity * self.call * self.discount(self.terms["payment_date"])

    def pv_historical_fix_call(self):
        payout = self.positive(
            self.terminal
            - self.terms["historical_price"] * self.terms["reset_multiplier"]
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_weighted_option_strip(self):
        payout = sum(
            (
                weight * self.positive(self.terminal - k)
                for k, weight in zip(self.terms["strikes"], self.terms["weights"])
            )
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_nested_loop_portfolio(self):
        payout = sum(
            (
                weight * self.positive(self.terminal - k - shift)
                for k, weight in zip(self.terms["strikes"], self.terms["weights"])
                for shift in self.terms["shifts"]
            )
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_sparse_vector_basket(self):
        selected = [self.prices[i] for i in self.terms["selected_observations"]]
        payout = self.positive(
            (selected[0] + 2 * selected[1] + 3 * selected[2]) / 6 - self.strike
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_conditional_vector(self):
        conditional_average = sum(
            price if price > self.strike else 0 for price in self.prices
        ) / len(self.prices)
        payout = self.positive(conditional_average - self.terms["average_strike"])
        return self.quantity * payout * self.discount(self.maturity)

    def pv_schedule_asian(self):
        payout = self.positive(self.average - self.strike)
        return self.quantity * payout * self.discount(self.maturity)

    def pv_schedule_fixed_coupon(self):
        previous = date.fromisoformat(self.terms["schedule_start"])
        pv = 0
        for d in self.observation_dates:
            current = date.fromisoformat(d)
            pv += (
                self.terms["notional"]
                * self.terms["fixed_rate"]
                * (current - previous).days
                / 365
                * self.discount(current)
            )
            previous = current
        return pv

    def pv_bermudan_put(self):
        exercise_dates = self.terms["exercise_dates"]
        exercise_prices = [self.spot(d) for d in exercise_dates]
        candidates = [0]
        for i, (d, price) in enumerate(zip(exercise_dates, exercise_prices)):
            amount = self.positive(self.strike - price)
            candidates.append(self.quantity * amount * self.discount(d))
        return max(candidates)

    def pv_bermudan_call(self):
        exercise_dates = self.terms["exercise_dates"]
        exercise_prices = [self.spot(d) for d in exercise_dates]
        candidates = [0]
        for i, (d, price) in enumerate(zip(exercise_dates, exercise_prices)):
            amount = self.positive(price - self.strike)
            candidates.append(self.quantity * amount * self.discount(d))
        return max(candidates)

    def pv_bermudan_asian_put(self):
        exercise_dates = self.terms["exercise_dates"]
        exercise_prices = [self.spot(d) for d in exercise_dates]
        candidates = [0]
        for i, (d, price) in enumerate(zip(exercise_dates, exercise_prices)):
            amount = self.positive(
                self.strike - sum(exercise_prices[: i + 1]) / (i + 1)
            )
            candidates.append(self.quantity * amount * self.discount(d))
        return max(candidates)

    def pv_conditional_bermudan_put(self):
        exercise_dates = self.terms["exercise_dates"]
        exercise_prices = [self.spot(d) for d in exercise_dates]
        candidates = [0]
        for i, (d, price) in enumerate(zip(exercise_dates, exercise_prices)):
            amount = self.positive(self.strike - price)
            if self.family == "conditional_bermudan_put" and (
                not price < self.terms["exercise_limit"]
            ):
                amount = 0
            candidates.append(self.quantity * amount * self.discount(d))
        return max(candidates)

    def pv_american_put_daily(self):
        exercise_dates = self.terms["exercise_dates"]
        exercise_prices = [self.spot(d) for d in exercise_dates]
        candidates = [0]
        for i, (d, price) in enumerate(zip(exercise_dates, exercise_prices)):
            amount = self.positive(self.strike - price)
            candidates.append(self.quantity * amount * self.discount(d))
        return max(candidates)

    def pv_american_call_daily(self):
        exercise_dates = self.terms["exercise_dates"]
        exercise_prices = [self.spot(d) for d in exercise_dates]
        candidates = [0]
        for i, (d, price) in enumerate(zip(exercise_dates, exercise_prices)):
            amount = self.positive(price - self.strike)
            candidates.append(self.quantity * amount * self.discount(d))
        return max(candidates)

    def pv_rate_fixed_bond(self):
        previous = date.fromisoformat(self.terms["start_date"])
        pv = 0
        for d in self.terms["payment_dates"]:
            current = date.fromisoformat(d)
            pv += (
                self.notional
                * self.fixed_rate
                * (current - previous).days
                / 365
                * self.discount(current)
            )
            previous = current
        return pv + self.notional * self.discount(self.maturity)

    def pv_floating_note(self):
        pv = 0
        for fixing, payment in zip(
            self.terms["fixing_dates"], self.terms["payment_dates"]
        ):
            fixing_dt, payment_dt = (
                date.fromisoformat(fixing),
                date.fromisoformat(payment),
            )
            libor = libor_zero_vol(fixing_dt, self.rate)
            amount = libor
            pv += (
                self.notional
                * (payment_dt - fixing_dt).days
                / 360
                * amount
                * self.discount(payment)
            )
        return pv + self.notional * self.discount(self.maturity)

    def pv_caplet(self):
        pv = 0
        for fixing, payment in zip(
            self.terms["fixing_dates"], self.terms["payment_dates"]
        ):
            fixing_dt, payment_dt = (
                date.fromisoformat(fixing),
                date.fromisoformat(payment),
            )
            libor = libor_zero_vol(fixing_dt, self.rate)
            amount = libor
            amount = self.positive(libor - self.fixed_rate)
            pv += (
                self.notional
                * (payment_dt - fixing_dt).days
                / 360
                * amount
                * self.discount(payment)
            )
        return pv + 0

    def pv_floorlet(self):
        pv = 0
        for fixing, payment in zip(
            self.terms["fixing_dates"], self.terms["payment_dates"]
        ):
            fixing_dt, payment_dt = (
                date.fromisoformat(fixing),
                date.fromisoformat(payment),
            )
            libor = libor_zero_vol(fixing_dt, self.rate)
            amount = libor
            amount = self.positive(self.fixed_rate - libor)
            pv += (
                self.notional
                * (payment_dt - fixing_dt).days
                / 360
                * amount
                * self.discount(payment)
            )
        return pv + 0

    def pv_cap(self):
        pv = 0
        for fixing, payment in zip(
            self.terms["fixing_dates"], self.terms["payment_dates"]
        ):
            fixing_dt, payment_dt = (
                date.fromisoformat(fixing),
                date.fromisoformat(payment),
            )
            libor = libor_zero_vol(fixing_dt, self.rate)
            amount = libor
            amount = self.positive(libor - self.fixed_rate)
            pv += (
                self.notional
                * (payment_dt - fixing_dt).days
                / 360
                * amount
                * self.discount(payment)
            )
        return pv + 0

    def pv_rate_corridor(self):
        return self.terms["coupon"] * sum(
            self.discount(d)
            for d in self.observation_dates
            if self.terms["rate_lower"]
            <= libor_zero_vol(date.fromisoformat(d), self.rate)
            <= self.terms["rate_upper"]
        )

    def pv_payer_swap(self):
        pv = 0
        for fixing, payment in zip(
            self.terms["fixing_dates"], self.terms["payment_dates"]
        ):
            fixing_dt, payment_dt = (
                date.fromisoformat(fixing),
                date.fromisoformat(payment),
            )
            libor = libor_zero_vol(fixing_dt, self.rate)
            amount = libor
            amount = libor - self.fixed_rate
            pv += (
                self.notional
                * (payment_dt - fixing_dt).days
                / 360
                * amount
                * self.discount(payment)
            )
        return pv + 0

    def pv_european_payer_swaption(self):
        annuity = sum(
            (
                w * math.exp(-self.rate * (self.time(d) - self.time(self.maturity)))
                for w, d in zip(
                    self.terms["annuity_weights"], self.terms["annuity_dates"]
                )
            )
        )
        return (
            self.notional
            * self.positive(swap_zero_vol(self.maturity, self.rate) - self.fixed_rate)
            * annuity
            * self.discount(self.maturity)
        )

    def pv_bermudan_bond_call(self):
        bond_time = self.time(self.terms["bond_maturity"])
        return max(
            [
                0,
                *(
                    self.notional
                    * self.positive(
                        math.exp(-self.rate * (bond_time - self.time(d)))
                        - self.terms["bond_strike"]
                    )
                    * self.discount(d)
                    for d in self.terms["exercise_dates"]
                ),
            ]
        )

    def pv_rate_vector_average(self):
        average_rate = sum(
            libor_zero_vol(date.fromisoformat(d), self.rate)
            for d in self.observation_dates
        ) / len(self.observation_dates)
        return (
            self.notional
            * self.positive(average_rate - self.fixed_rate)
            * self.discount(self.maturity)
        )

    def pv_multi_asset_basket(self):
        first = (
            self.market["spots"][0]
            * self.multiplier
            * math.exp((self.rate - self.market["divs"][0]) * self.time(self.maturity))
        )
        second = (
            self.market["spots"][1]
            * self.multiplier
            * math.exp((self.rate - self.market["divs"][1]) * self.time(self.maturity))
        )
        payout = self.positive(
            self.terms["basket_weight"] * first
            + (1 - self.terms["basket_weight"]) * second
            - self.strike
        )
        return self.quantity * payout * self.discount(self.maturity)

    def pv_hybrid_equity_rate(self):
        return self.quantity * self.call * self.discount(self.terms["bond_maturity"])


REFERENCE_PAYOFFS = {
    name.removeprefix("pv_"): method
    for name, method in vars(DeterministicPayoff).items()
    if name.startswith("pv_")
}


def reference_pv(record, scenario):
    payoff = DeterministicPayoff(record, scenario)
    return REFERENCE_PAYOFFS[record["family"]](payoff)

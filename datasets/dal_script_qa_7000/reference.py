from __future__ import annotations

import calendar
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
    return ((end.year - start.year) * 360 + (end.month - start.month) * 30 + end_day - start_day) / 360


def swap_zero_vol(start, rate):
    maturity = add_months(start, 12)
    unadjusted = [start]
    while add_months(unadjusted[-1], 6) <= maturity:
        unadjusted.append(add_months(unadjusted[-1], 6))
    if unadjusted[-1] != maturity:
        unadjusted.append(maturity)
    endpoints = [modified_following(d) for d in unadjusted]
    annuity = sum(basis_30_360(a, b) * math.exp(-rate * (b - start).days / 365) for a, b in zip(endpoints, endpoints[1:]))
    floating_pv = math.exp(-rate * (endpoints[0] - start).days / 365) - math.exp(-rate * (endpoints[-1] - start).days / 365)
    return floating_pv / annuity


def reference_pv(record, scenario):
    family = record["family"]
    terms = record["contract_terms"]
    market = record["pricing_context"]["model"]
    today = date.fromisoformat(record["pricing_context"]["evaluation_date"])
    maturity = date.fromisoformat(terms["maturity"])
    rate = scenario["rate"]
    multiplier = scenario["spot_multiplier"]
    quantity, strike = terms["quantity"], terms["strike"]
    initial = terms["initial_price"]
    dividend = market.get("div", 0.01)

    def time(value):
        value = date.fromisoformat(value) if isinstance(value, str) else value
        return (value - today).days / 365

    def discount(value):
        return math.exp(-rate * time(value))

    def spot(value):
        return initial * multiplier * math.exp((rate - dividend) * time(value))

    def positive(value):
        return max(value, 0.0)

    terminal = spot(maturity)
    call, put = positive(terminal - strike), positive(strike - terminal)
    payout = None
    if family == "european_call":
        payout = call
    elif family == "european_put":
        payout = put
    elif family.startswith("cash_digital") or family.startswith("asset_digital"):
        triggered = terminal > strike if family.endswith("call") else terminal < strike
        payout = (terms["digital_cash"] if family.startswith("cash") else terminal) if triggered else 0
    elif family == "forward_long":
        payout = terminal - strike
    elif family == "forward_short":
        payout = strike - terminal
    elif family == "bull_call_spread":
        payout = call - positive(terminal - strike - terms["width"])
    elif family == "bear_put_spread":
        payout = positive(strike + terms["width"] - terminal) - put
    elif family == "straddle":
        payout = call + put
    elif family == "strangle":
        payout = put + positive(terminal - strike - terms["width"])
    elif family == "butterfly":
        width = terms["width"]
        payout = call - 2 * positive(terminal - strike - width) + positive(terminal - strike - 2 * width)
    elif family == "call_ratio_spread":
        payout = call - 2 * positive(terminal - strike - terms["width"])
    elif family == "capped_call":
        payout = min(call, terms["cap"])
    elif family == "interval_cash":
        payout = terms["digital_cash"] if terms["lower"] <= terminal <= terms["upper"] else 0
    elif family == "tiered_cash":
        if terminal == strike:
            payout = terms["digital_cash"]
        elif strike < terminal < strike + terms["width"]:
            payout = 2 * terms["digital_cash"]
        elif terminal >= strike + terms["width"]:
            payout = 3 * terms["digital_cash"]
        else:
            payout = 0
    elif family == "power_call":
        payout = positive(terminal * terminal - strike * strike)
    elif family == "log_contract":
        payout = math.log(terminal / initial)
    elif family == "geometric_return_call":
        payout = positive(terminal / initial - 1 - terms["return_strike"])
    elif family == "delayed_settlement_call":
        return quantity * call * discount(terms["payment_date"])
    elif family == "historical_fix_call":
        payout = positive(terminal - terms["historical_price"] * terms["reset_multiplier"])
    elif family == "weighted_option_strip":
        payout = sum(weight * positive(terminal - k) for k, weight in zip(terms["strikes"], terms["weights"]))
    elif family == "nested_loop_portfolio":
        payout = sum(weight * positive(terminal - k - shift) for k, weight in zip(terms["strikes"], terms["weights"]) for shift in terms["shifts"])
    elif family == "forward_start_call":
        payout = positive(terminal - terms["moneyness"] * spot(terms["reset_date"]))
    elif family == "equity_linked_note":
        principal = terms["principal"] if terminal >= terms["trigger"] else terms["principal"] * terminal / terms["trigger"]
        return (principal + terms["coupon"]) * discount(maturity)
    elif family == "multi_asset_basket":
        first = market["spots"][0] * multiplier * math.exp((rate - market["divs"][0]) * time(maturity))
        second = market["spots"][1] * multiplier * math.exp((rate - market["divs"][1]) * time(maturity))
        payout = positive(terms["basket_weight"] * first + (1 - terms["basket_weight"]) * second - strike)
    elif family == "hybrid_equity_rate":
        return quantity * call * discount(terms["bond_maturity"])
    if payout is not None:
        return quantity * payout * discount(maturity)

    observation_dates = terms.get("observation_dates", [])
    prices = [spot(d) for d in observation_dates]
    average = sum(prices) / len(prices) if prices else 0
    if family in ("arithmetic_asian_call", "schedule_asian"):
        payout = positive(average - strike)
    elif family == "arithmetic_asian_put":
        payout = positive(strike - average)
    elif family == "geometric_asian_call":
        geometric_average = math.prod(prices) ** (1 / len(prices))
        payout = positive(geometric_average - strike)
    elif family == "weighted_asian":
        payout = positive(sum(s * w for s, w in zip(prices, terms["weights"])) / sum(terms["weights"]) - strike)
    elif family == "floating_asian_call":
        payout = positive(terminal - average)
    elif family == "lookback_fixed_call":
        payout = positive(max(prices) - strike)
    elif family == "lookback_fixed_put":
        payout = positive(strike - min(prices))
    elif family == "lookback_float_call":
        payout = terminal - min(prices)
    elif family == "range_option":
        payout = max(prices) - min(prices)
    elif family in ("cliquet", "capped_cliquet", "realized_variance", "realized_volatility"):
        # The contractual initial reference stays fixed when the valuation spot is changed.
        consecutive = [initial, *prices]
        returns = [b / a - 1 for a, b in zip(consecutive, consecutive[1:])]
        if family == "cliquet":
            payout = sum(returns)
        elif family == "capped_cliquet":
            payout = positive(sum(min(max(r, terms["local_floor"]), terms["local_cap"]) for r in returns))
        else:
            payout = sum(math.log(b / a) ** 2 for a, b in zip(consecutive, consecutive[1:])) * terms["annualization"]
            if family == "realized_volatility":
                payout = math.sqrt(payout)
    elif family in ("up_out_call", "down_out_put", "up_in_call", "down_in_put", "double_out_call", "double_in_put"):
        if family.startswith("double"):
            hit = any(price <= terms["lower"] or price >= terms["upper"] for price in prices)
        elif family.startswith("up"):
            hit = any(price >= terms["upper"] for price in prices)
        else:
            hit = any(price <= terms["lower"] for price in prices)
        payout = call if family.endswith("call") else put
        payout *= (not hit) if "out" in family else hit
    elif family == "autocall":
        for d, price in zip(observation_dates, prices):
            if price >= terms["trigger"]:
                return (terms["principal"] + terms["coupon"]) * discount(d)
        return terms["principal"] * discount(maturity)
    elif family == "memory_coupon":
        memory, pv = 0, 0
        for d, price in zip(observation_dates, prices):
            memory += terms["coupon"]
            if price >= terms["trigger"]:
                pv += memory * discount(d)
                memory = 0
        return pv + terms["principal"] * discount(maturity)
    elif family == "corridor_coupon":
        return quantity * terms["coupon"] * sum(discount(d) for d, price in zip(observation_dates, prices) if terms["lower"] <= price <= terms["upper"])
    elif family == "sparse_vector_basket":
        selected = [prices[i] for i in terms["selected_observations"]]
        payout = positive((selected[0] + 2 * selected[1] + 3 * selected[2]) / 6 - strike)
    elif family == "conditional_vector":
        conditional_average = sum(price if price > strike else 0 for price in prices) / len(prices)
        payout = positive(conditional_average - terms["average_strike"])
    elif family == "schedule_fixed_coupon":
        previous = date.fromisoformat(terms["schedule_start"])
        pv = 0
        for d in observation_dates:
            current = date.fromisoformat(d)
            pv += terms["notional"] * terms["fixed_rate"] * (current - previous).days / 365 * discount(current)
            previous = current
        return pv
    if payout is not None:
        return quantity * payout * discount(maturity)

    if family in ("bermudan_put", "bermudan_call", "bermudan_asian_put", "conditional_bermudan_put", "american_put_daily", "american_call_daily"):
        exercise_dates = terms["exercise_dates"]
        exercise_prices = [spot(d) for d in exercise_dates]
        candidates = [0]
        for i, (d, price) in enumerate(zip(exercise_dates, exercise_prices)):
            if family == "bermudan_asian_put":
                amount = positive(strike - sum(exercise_prices[:i + 1]) / (i + 1))
            else:
                amount = positive(strike - price) if "put" in family else positive(price - strike)
            if family == "conditional_bermudan_put" and not price < terms["exercise_limit"]:
                amount = 0
            candidates.append(quantity * amount * discount(d))
        return max(candidates)

    notional, fixed_rate = terms["notional"], terms["fixed_rate"]
    if family == "rate_fixed_bond":
        previous = date.fromisoformat(terms["start_date"])
        pv = 0
        for d in terms["payment_dates"]:
            current = date.fromisoformat(d)
            pv += notional * fixed_rate * (current - previous).days / 365 * discount(current)
            previous = current
        return pv + notional * discount(maturity)
    if family in ("floating_note", "caplet", "floorlet", "cap", "payer_swap"):
        pv = 0
        for fixing, payment in zip(terms["fixing_dates"], terms["payment_dates"]):
            fixing_dt, payment_dt = date.fromisoformat(fixing), date.fromisoformat(payment)
            libor = libor_zero_vol(fixing_dt, rate)
            amount = libor
            if family in ("cap", "caplet"):
                amount = positive(libor - fixed_rate)
            elif family == "floorlet":
                amount = positive(fixed_rate - libor)
            elif family == "payer_swap":
                amount = libor - fixed_rate
            pv += notional * (payment_dt - fixing_dt).days / 360 * amount * discount(payment)
        return pv + (notional * discount(maturity) if family == "floating_note" else 0)
    if family == "rate_corridor":
        return terms["coupon"] * sum(discount(d) for d in observation_dates if terms["rate_lower"] <= libor_zero_vol(date.fromisoformat(d), rate) <= terms["rate_upper"])
    if family == "european_payer_swaption":
        annuity = sum(w * math.exp(-rate * (time(d) - time(maturity))) for w, d in zip(terms["annuity_weights"], terms["annuity_dates"]))
        return notional * positive(swap_zero_vol(maturity, rate) - fixed_rate) * annuity * discount(maturity)
    if family == "bermudan_bond_call":
        bond_time = time(terms["bond_maturity"])
        return max([0, *(notional * positive(math.exp(-rate * (bond_time - time(d))) - terms["bond_strike"]) * discount(d) for d in terms["exercise_dates"])])
    if family == "rate_vector_average":
        average_rate = sum(libor_zero_vol(date.fromisoformat(d), rate) for d in observation_dates) / len(observation_dates)
        return notional * positive(average_rate - fixed_rate) * discount(maturity)
    raise ValueError(f"No independent reference for {family}")

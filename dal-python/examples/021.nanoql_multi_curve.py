#!/usr/bin/env python3
"""Calibrate EUR discount and projection curves and value two payer swaps."""

from datetime import date, timedelta
from time import perf_counter
from typing import NamedTuple

import dal

TODAY = date(2012, 12, 11)
NOTIONAL = 1_000_000
FIXED_RATE = 0.007
BASE_QUOTE = 0.00762
SHOCKED_QUOTE = 0.0090
TOLERANCE = 1e-11

FRA_QUOTES = (
    0.002930,
    0.002720,
    0.002600,
    0.002560,
    0.002520,
    0.002480,
    0.002540,
    0.002610,
    0.002670,
    0.002790,
    0.002910,
    0.003030,
    0.003180,
    0.003350,
    0.003520,
    0.003710,
    0.003890,
    0.004090,
)
OIS_QUOTES = (
    (15, 0.00002),
    (18, 0.00008),
    (21, 0.00021),
    (24, 0.00036),
    (36, 0.00127),
    (48, 0.00274),
    (60, 0.00456),
    (72, 0.00647),
    (84, 0.00827),
    (96, 0.00996),
    (108, 0.01147),
    (120, 0.01280),
    (132, 0.01404),
    (144, 0.01516),
    (180, 0.01764),
    (240, 0.01939),
    (300, 0.02003),
    (360, 0.02038),
)
IRS_QUOTES = (
    (3, 0.004240),
    (4, 0.005760),
    (5, BASE_QUOTE),
    (6, 0.009540),
    (7, 0.011350),
    (8, 0.013030),
    (9, 0.014520),
    (10, 0.015840),
    (12, 0.018090),
    (15, 0.020370),
    (20, 0.021870),
    (25, 0.022340),
    (30, 0.022560),
    (35, 0.022950),
    (40, 0.023480),
    (50, 0.024210),
    (60, 0.024630),
)


class Quote(NamedTuple):
    kind: str
    label: str
    start: date
    end: date
    rate: float


def dal_date(value):
    return dal.Date_(value.year, value.month, value.day)


def add_months(value, months):
    # All month advances in this example begin before the 29th.
    year, month = divmod(value.year * 12 + value.month - 1 + months, 12)
    return value.replace(year=year, month=month + 1)


def adjust(value, calendar, convention=dal.BizDayConvention_.FOLLOWING):
    result = dal.Adjust(calendar, dal_date(value), convention)
    return date(dal.Year(result), dal.Month(result), dal.Day(result))


def advance_business_days(value, days, calendar):
    for _ in range(days):
        value = adjust(value + timedelta(days=1), calendar)
    return value


def estr_quotes(spot, calendar):
    quotes = []
    for lag, label in enumerate(("Overnight", "Tomorrow-next", "Spot-next")):
        start = advance_business_days(TODAY, lag, calendar)
        quotes.append(Quote("Deposit", label, start, advance_business_days(start, 1, calendar), 0.0004))
    quotes.extend(
        Quote("OIS", f"OIS {weeks}W", spot, spot + timedelta(weeks=weeks), rate)
        for weeks, rate in enumerate((0.00070, 0.00069, 0.00078), 1)
    )
    quotes.append(Quote("OIS", "OIS 1M", spot, add_months(spot, 1), 0.00074))
    meetings = (
        date(2013, 1, 16),
        date(2013, 2, 13),
        date(2013, 3, 13),
        date(2013, 4, 10),
        date(2013, 5, 8),
        date(2013, 6, 12),
    )
    quotes.extend(
        Quote("OIS", f"ECB {start:%b %Y}", start, end, rate)
        for start, end, rate in zip(
            meetings, meetings[1:], (0.000460, 0.000160, -0.000070, -0.000130, -0.000140)
        )
    )
    quotes.extend(
        Quote("OIS", f"OIS {months}M", spot, add_months(spot, months), rate) for months, rate in OIS_QUOTES
    )
    return quotes


def euribor_quotes(spot, calendar, five_year_quote):
    modified = dal.BizDayConvention_.MODIFIED_FOLLOWING
    start = advance_business_days(TODAY, 3, calendar)
    quotes = [Quote("Deposit", "Deposit 6M", start, adjust(add_months(start, 6), calendar), 0.00312)]
    for months, rate in enumerate(FRA_QUOTES, 1):
        start = adjust(add_months(spot, months), calendar, modified)
        end = adjust(add_months(start, 6), calendar, modified)
        quotes.append(Quote("FRA", f"FRA {months}×{months + 6}", start, end, rate))
    quotes.extend(
        Quote(
            "IRS",
            f"IRS {years}Y",
            spot,
            add_months(spot, 12 * years),
            five_year_quote if years == 5 else rate,
        )
        for years, rate in IRS_QUOTES
    )
    return quotes


def rate_leg(calendar, frequency, basis, accrual, payment):
    leg = dal.RateLegConvention_New(dal.PeriodLength_(frequency), dal.DayBasis_(basis))
    leg.accrual_holidays = leg.payment_holidays = calendar
    leg.business_day_convention = accrual
    leg.payment_convention = payment
    return leg


def calibrate(anchor, quotes, calendar, discount=None):
    following = dal.BizDayConvention_.FOLLOWING
    modified = dal.BizDayConvention_.MODIFIED_FOLLOWING
    unadjusted = dal.BizDayConvention_.UNADJUSTED
    tenor = dal.PeriodLength_("6M")
    collateral = dal.CollateralType_OIS()
    act360 = dal.DayBasis_("ACT_360")
    overnight = dal.RateIndexConvention_New(dal.PeriodLength_("1M"), act360, collateral)
    deposit_index = dal.RateIndexConvention_New(tenor, act360, collateral, discount is not None)
    euribor = dal.RateIndexConvention_New(tenor, act360, collateral, True)
    ois_leg = rate_leg(calendar, "12M", "ACT_360", modified, following)
    fixed_leg = rate_leg(calendar, "12M", "BOND", unadjusted, modified)
    float_leg = rate_leg(calendar, "6M", "ACT_360", modified, modified)
    instrument_builders = {
        "Deposit": (dal.Deposit_New, (deposit_index,)),
        "OIS": (dal.OISSwap_New, (ois_leg, overnight, ois_leg)),
        "FRA": (dal.FRA_New, (euribor,)),
        "IRS": (dal.Swap_New, (fixed_leg, euribor, float_leg)),
    }
    trade_date = dal_date(anchor)
    instruments = []
    for kind, _, start, end, rate in quotes:
        build_instrument, conventions = instrument_builders[kind]
        instruments.append(build_instrument(trade_date, dal_date(start), dal_date(end), rate, *conventions))

    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = trade_date
    builder.ccy_ = dal.String_("EUR")
    builder.curveName_ = dal.String_("ESTR" if discount is None else "Euribor 6M")
    builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
    builder.logDfScheme_ = dal.LogDfScheme.LOG_CUBIC_NATURAL
    builder.initialGuess_ = 0.01
    builder.tolerance_ = builder.fitTolerance_ = TOLERANCE
    builder.instruments_ = instruments
    builder.knotDates_ = [trade_date] + [
        dal_date(knot) for knot in sorted(adjust(q.end, calendar) for q in quotes)
    ]
    if discount is not None:
        builder.calibrateDiscountCurve_ = False
        builder.targetTenor_ = tenor
        builder.discountCurves_ = {collateral: discount}
    options = dal.CurveCalibrationOptions_()
    options.compute_eff_jacobian_inverse = False
    options.compute_forward_jacobian = False
    result = dal.CalibrateSingleCurve(builder.Build(), options)
    if not result.diagnostics_.max_abs_residual <= TOLERANCE:
        raise RuntimeError(f"{builder.curveName_} did not recover the market quotes")
    return result


def swap_periods(start, calendar):
    modified = dal.BizDayConvention_.MODIFIED_FOLLOWING
    fixed_dates = [adjust(add_months(start, months), calendar, modified) for months in range(12, 61, 12)]
    float_dates = [adjust(add_months(start, months), calendar, modified) for months in range(0, 61, 6)]
    return fixed_dates, list(zip(float_dates, float_dates[1:]))


def price_swap(start, calendar, discount, projection):
    fixed_dates, float_periods = swap_periods(start, calendar)
    today = dal_date(TODAY)
    # All unadjusted annual fixed accrual intervals run from the 13th to the 13th.
    fixed_annuity = sum(discount(today, dal_date(payment)) for payment in fixed_dates)
    float_annuity = float_pv = 0.0
    for first, last in float_periods:
        df = discount(today, dal_date(last))
        float_annuity += df * (last - first).days / 360
        float_pv += df * (1 / projection(dal_date(first), dal_date(last)) - 1)
    pv_per_unit = float_pv - FIXED_RATE * fixed_annuity
    return NOTIONAL * pv_per_unit, -pv_per_unit / float_annuity, float_pv / fixed_annuity


def print_swap_prices(prices):
    print(
        f"\n{'5Y input / trade':<20}{'DAL NPV (EUR)':>15}{'NanoQL (EUR)':>15}{'Spread (%)':>14}{'Rate (%)':>12}"
    )
    print("-" * 76)
    reference = (
        ("0.762%  5Y spot", 3076.04, -0.0006, 0.0076),
        ("0.762%  1Y x 5Y", 19202.54, -0.0038, 0.0109),
        ("0.900%  5Y spot", 9922.71, -0.0020, 0.0090),
        ("0.900%  1Y x 5Y", 19202.54, -0.0038, 0.0109),
    )
    for (name, expected_pv, expected_spread, expected_rate), (pv, spread, rate) in zip(reference, prices):
        print(f"{name:<20}{pv:>15.2f}{expected_pv:>15.2f}{100 * spread:>14.5f}{100 * rate:>12.5f}")
        if not (
            abs(pv - expected_pv) < 0.005
            and abs(spread - expected_spread) < 0.00005
            and abs(rate - expected_rate) < 0.00005
        ):
            raise RuntimeError(f"{name} differs from the published value")


def main():
    timer = perf_counter()
    calendar = dal.Holidays_("TARGET")
    spot = advance_business_days(TODAY, 2, calendar)
    forward = adjust(add_months(spot, 12), calendar)
    discount = calibrate(TODAY, estr_quotes(spot, calendar), calendar)
    projections = [
        calibrate(spot, euribor_quotes(spot, calendar, quote), calendar, discount.curve_)
        for quote in (BASE_QUOTE, SHOCKED_QUOTE)
    ]
    prices = [
        price_swap(start, calendar, discount.curve_, result.curve_)
        for result in projections
        for start in (spot, forward)
    ]
    elapsed_ms = 1000 * (perf_counter() - timer)

    print(f"EUR multi-curve example, {TODAY}, natural cubic log-DF interpolation")
    for result in (discount, *projections):
        diag = result.diagnostics_
        print(
            f"{diag.curve_name}: {len(diag.market_rates)} quotes, max absolute residual {1e4 * diag.max_abs_residual:.2e} bp"
        )
    print_swap_prices(prices)
    print(f"\nCalibration and pricing time: {elapsed_ms:.3f} ms")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Fit GSR to an input OIS curve and price a swap and European swaption."""

import math

import dal


TODAY = dal.Date_(2026, 9, 28)
EXPIRY = dal.Date_(2027, 9, 28)
FIRST_FLOAT_COUPON = dal.Date_(2027, 12, 28)
FIRST_COUPON = dal.Date_(2028, 3, 28)
THIRD_FLOAT_COUPON = dal.Date_(2028, 6, 28)
MATURITY = dal.Date_(2028, 9, 28)
INPUT_RATE = 0.025
STRIKE = 0.03
PATHS = 1 << 16


def price(product, model):
    return dal.MonteCarlo_ValueWithSettings(
        product,
        model,
        PATHS,
        valuation=dal.ScriptValuationSettings_(evaluation_date=TODAY),
        simulation=dal.MonteCarloSettings_(compiled=True),
    )["PV"]


def print_script(name, product):
    print(f"### {name}\n\n```text")
    print(dal.Product_DebugTree(product, ascii=True, width=100).rstrip())
    print("```\n")


def standard_payer_swap():
    # USD vanilla IRS: quarterly ACT/360 Libor coupons, semiannual 30/360 fixed coupons.
    dates = [EXPIRY, FIRST_FLOAT_COUPON, FIRST_COUPON, THIRD_FLOAT_COUPON, MATURITY]
    fixing_dates = ["2027-09-24", "2027-12-24", "2028-03-24", "2028-06-26"]
    scripts = [f"{STRIKE}"]
    for i, (start, end, fixing) in enumerate(zip(dates[:-1], dates[1:], fixing_dates), 1):
        fixed_coupon = " - 0.5 * STRIKE" if i % 2 == 0 else ""
        scripts.append(
            f"pay PAYS ({end - start}.0 / 360.0) * "
            f"FIX(IR[USD,LIBOR_3M_CME,{start}],{fixing}){fixed_coupon}"
        )
    return dal.Product_New(["STRIKE", *dates[1:]], scripts)


def static_swap_price(discount_curve):
    fixed_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("6M"), dal.DayBasis_New("30_360")
    )
    float_leg = dal.RateLegConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360")
    )
    float_index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("3M"), dal.DayBasis_New("ACT_360"), dal.CollateralType_OIS()
    )
    float_index.fixing_lag = 2
    identity = dal.FixingIdentity_()
    identity.index_name = "USD-LIBOR-3M"
    identity.fixing_hour, identity.fixing_minute = 11, 0
    terms = dal.FixedFloatTradeTerms_(
        notional=1.0,
        contract_rate=STRIKE,
        pay_fixed=True,
        fixed_leg=fixed_leg,
        float_leg=float_leg,
        float_index=float_index,
        fixing_identity=identity,
        forecast_component_key="curve",
        discount_component_key="curve",
    )
    trade = dal.RateTradeDefinition_(
        instrument_id="forward_payer_swap",
        instrument_type=dal.RateInstrumentType.IRS,
        trade_date=TODAY,
        start_date=EXPIRY,
        maturity_date=MATURITY,
        currency="USD",
        terms=dal.IrsTradeTerms_(value=terms),
    )
    market = dal.RatePricingMarket_(
        valuation_time=dal.DateTime_(TODAY, 9, 0),
        result_currency="USD",
        curve_components={"curve": discount_curve},
        fixings=dal.MarketFixingSnapshot_New({}),
    )
    result = dal.PriceRateTrades(trades=[trade], market=market)[0]
    if not result.succeeded:
        raise RuntimeError(f"Static IRS pricing failed: {result.error}")
    return result.pv


def main():
    dal.EvaluationDate_Set(TODAY)
    dates = [TODAY, EXPIRY, FIRST_COUPON, MATURITY]
    log_discount_factors = [-INPUT_RATE * (date - TODAY) / 365.0 for date in dates]
    discount_curve = dal.DiscountLogDF_New("input_usd_ois", "USD", dates, log_discount_factors)
    yield_curve = dal.CurveBlock_New(discount_curve)
    curve = dal.GSRCurveDataFromYieldCurve_New("gsr_curve", yield_curve, TODAY, dates, [])
    vol = dal.GSRVolData_New("gsr_vol", [TODAY], [0.02], [TODAY], [1.0])
    model = dal.GSRModelData_New("gsr", curve, vol)

    print("# GSR swap and swaption example\n")
    print("The GSR curve snapshots the input USD OIS yield curve; g and H are supplied, not calibrated.\n")
    print("| Curve node | Input DF | GSR P(0,T) |\n|---|---:|---:|")
    for date in dates[1:]:
        bond = dal.Product_New([TODAY], [f"pay PAYS FIX(IR[USD,DF,{date}])"])
        input_df = discount_curve(TODAY, date)
        model_df = price(bond, model)
        if not math.isclose(model_df, input_df, rel_tol=0.0, abs_tol=1e-10):
            raise RuntimeError(f"GSR did not fit the input discount curve at {date}")
        print(f"| {date} | {input_df:.9f} | {model_df:.9f} |")

    print(f"\n| g knot | g | H knot | H |\n|---|---:|---|---:|\n| {TODAY} | 0.020000 | {TODAY} | 1.000000 |\n")

    # The swaption's cash payoff uses the present value of the standard swap's fixed annuity.
    annuity = "0.5 * FIX(IR[USD,DF,2028-03-28]) + 0.5 * FIX(IR[USD,DF,2028-09-28])"
    swap_value = f"(FIX(IR[USD,SWAP,1Y,2027-09-28]) - STRIKE) * ({annuity})"
    swap = standard_payer_swap()
    swaption = dal.Product_New(["STRIKE", EXPIRY], [f"{STRIKE}", f"pay PAYS MAX({swap_value}, 0)"])

    print_script("Standard forward payer swap", swap)
    print_script("Cash-settled European payer swaption", swaption)

    swap_pv = price(swap, model)
    swaption_pv = price(swaption, model)
    if not math.isfinite(swap_pv) or swap_pv >= 0.0 or swaption_pv <= 0.0:
        raise RuntimeError("Unexpected GSR swap or swaption price")
    static_swap_pv = static_swap_price(discount_curve)
    curve_identity = (
        discount_curve(TODAY, EXPIRY)
        - discount_curve(TODAY, MATURITY)
        - STRIKE * 0.5 * (discount_curve(TODAY, FIRST_COUPON) + discount_curve(TODAY, MATURITY))
    )
    if not math.isclose(static_swap_pv, curve_identity, rel_tol=0.0, abs_tol=1e-10):
        raise RuntimeError("Static IRS PV differs from the yield-curve identity")
    if not math.isclose(swap_pv, static_swap_pv, rel_tol=0.0, abs_tol=2e-4):
        raise RuntimeError("Standard swap GSR PV differs from its input-curve PV")
    print(f"Forward start {EXPIRY}; unit notional; fixed rate {STRIKE:.2f}. ")
    print("The swap pays coupons on their scheduled dates; the swaption settles in cash at expiry.\n")
    print("| Product | GSR Monte Carlo PV | Static YieldCurve PV | Difference |")
    print("|---|---:|---:|---:|")
    print(f"| Standard forward payer swap | {swap_pv:.9f} | {static_swap_pv:.9f} | {swap_pv - static_swap_pv:.9f} |")
    print(f"| Cash-settled European payer swaption | {swaption_pv:.9f} | N/A | N/A |")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

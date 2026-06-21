#!/usr/bin/env python3
"""
006.xccy_calibration.py — Cross-currency basis calibration

Mirrors dal-cpp/examples/xccy_curve_calibration/xccy_curve_calibration.cpp.

Demonstrates:
  - Building USD and EUR OIS discount curves
  - Creating cross-currency basis swaps at various maturities
  - Calibrating the cross-currency basis curve
  - Printing market vs model spreads
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

import dal as _dal


def make_flat_curve(ccy, today, rate):
    """Build a flat discount curve."""
    knot_dates = [
        today.AddDays(365),
        today.AddDays(730),
        today.AddDays(1095),
        today.AddDays(1825),
        today.AddDays(3650),
        today.AddDays(7300),
        today.AddDays(10950),
    ]
    fwd_rates = [rate] * len(knot_dates)
    return _dal.DiscountPWLF_New(ccy, ccy, knot_dates, fwd_rates)


def make_xccy_block(ccy, today, rate):
    """Build a CurveBlock_ from a single discount curve."""
    curve = make_flat_curve(ccy, today, rate)
    return _dal.CurveBlock_New(curve)


def make_xccy_index():
    """Build a 12M float index convention for cross-currency swaps."""
    return _dal.RateIndexConvention_New(
        _dal.PeriodLength_("12M"),
        _dal.DayBasis_("ACT_365F"),
        _dal.CollateralType_OIS(),
        True)


def make_xccy_leg():
    """Build a 12M payment leg convention for cross-currency swaps."""
    return _dal.RateLegConvention_New(
        _dal.PeriodLength_("12M"),
        _dal.DayBasis_("ACT_365F"))


def main():
    _dal.EvaluationDate_Set(_dal.Date_(2024, 1, 15))
    today = _dal.Date_(2024, 1, 15)

    print("=" * 70)
    print("  Cross-currency basis calibration example")
    print("=" * 70)

    # ------------------------------------------------------------------
    # Build USD and EUR OIS curve blocks
    # ------------------------------------------------------------------
    usd_rate = 0.02   # 2% USD OIS
    eur_rate = 0.01   # 1% EUR OIS
    fx_spot = 1.10    # EURUSD = 1.10

    usd_block = make_xccy_block("USD", today, usd_rate)
    eur_block = make_xccy_block("EUR", today, eur_rate)

    print(f"\n  USD OIS rate: {usd_rate * 100:.2f}%")
    print(f"  EUR OIS rate: {eur_rate * 100:.2f}%")
    print(f"  FX spot (EURUSD): {fx_spot}")

    # ------------------------------------------------------------------
    # Build cross-currency basis swap conventions
    # ------------------------------------------------------------------
    domestic_index = make_xccy_index()
    domestic_leg = make_xccy_leg()
    foreign_index = make_xccy_index()
    foreign_leg = make_xccy_leg()

    currencies = _dal.CurrencyPair_New("USD", "EUR")

    # ------------------------------------------------------------------
    # Create cross-currency swaps at selected maturities
    # Note: market-consistent spreads require Precompute(), which is not
    # exposed in the Python bindings. For this demonstration we calibrate
    # a single instrument at zero spread with 2 knots (square system).
    # ------------------------------------------------------------------
    maturities_months = [24]  # single 2Y instrument
    basis_rate = 0.0

    instruments = []
    for m in maturities_months:
        maturity = today.AddDays(m * 30)
        inst = _dal.CrossCurrencySwap_New(
            today, today, maturity,
            basis_rate,
            currencies,
            110.0, 100.0,  # domestic/foreign notional
            domestic_leg, domestic_index,
            foreign_leg, foreign_index)
        instruments.append(inst)

    print(f"\n  Instruments: {len(instruments)} cross-currency swap ({maturities_months[0]}M)")
    print(f"  Market basis spread: {basis_rate * 10000:.1f} bp")

    # ------------------------------------------------------------------
    # Build calibration spec
    # ------------------------------------------------------------------
    knot_dates = [
        today.AddDays(365),   #  1Y
        today.AddDays(730),   #  2Y
    ]

    xccy_builder = _dal.CrossCurrencyCalibrationSpecBuilder_()
    xccy_builder.today_ = today
    xccy_builder.basisPair_ = currencies
    xccy_builder.fxSpot_ = fx_spot
    xccy_builder.domesticCurveBlock_ = usd_block
    xccy_builder.foreignCurveBlock_ = eur_block
    xccy_builder.instruments_ = instruments
    xccy_builder.knotDates_ = knot_dates
    xccy_builder.smoothingWeight_ = 1.0
    xccy_builder.tolerance_ = 1e-10
    xccy_builder.fitTolerance_ = 1e-6
    xccy_spec = xccy_builder.Build()

    # ------------------------------------------------------------------
    # Run cross-currency calibration
    # ------------------------------------------------------------------
    result = _dal.CalibrateXccyMarket_New(xccy_spec)
    diag = result.diagnostics_

    # ------------------------------------------------------------------
    # Print diagnostics
    # ------------------------------------------------------------------
    print(f"\n  --- Calibration residuals ---")
    print(f"  {'Instrument':<20} {'Market(bp)':>12} {'Model(bp)':>12} {'Error(bp)':>12}")
    print(f"  {'-' * 56}")
    for i, m in enumerate(maturities_months):
        label = f"XCCY Swap {m}M"
        print(f"  {label:<20} {diag.marketRates_[i] * 10000:>12.4f} "
              f"{diag.modelRates_[i] * 10000:>12.4f} {diag.residuals_[i] * 10000:>12.6f}")

    print(f"\n  FX spot: {fx_spot}")
    print(f"  Max abs residual: {diag.maxAbsResidual_ * 10000:.6f} bp")
    print(f"  RMS residual:     {diag.rmsResidual_ * 10000:.6f} bp")

    # Print FX forward curve
    fxfwd = result.fxForwardCurve_
    print(f"\n  --- FX Forward Curve ({fxfwd.pair_}) ---")
    print(f"  {'Date':<14} {'Forward':>12}")
    print(f"  {'-' * 26}")
    for i in range(len(fxfwd.dates_)):
        print(f"  {str(fxfwd.dates_[i]):<14} {fxfwd.forwards_[i]:>12.6f}")

    print(f"\n  All diagnostics printed successfully.")


if __name__ == "__main__":
    main()

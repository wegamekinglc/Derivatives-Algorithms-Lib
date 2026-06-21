#!/usr/bin/env python3
"""
004.curve_calibration.py — Exactly-determined (square) multi-curve calibration

Mirrors dal-cpp/examples/curve_calibration/curve_calibration.cpp.

Calibrates in two sequential stages:
  Stage 1: OIS discount curve — 9 instruments on 9 knot pillars
  Stage 2: Libor 3M forward curve — 9 instruments on 9 knot pillars

Each stage is a square (exactly-determined) system:
  n_instruments == n_free_params  →  zero degrees of freedom.
  The EXACT solver interpolates through every quote.
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

import dal

# def_readwrite String_ fields need explicit wrapping (plain str works for functions)
S = dal.String_


def make_flat_discount_curve(name, ccy, today, rate):
    """Build a flat discount curve using DiscountPWLF_New for synthetic market data."""
    knot_dates = [
        today.AddDays(30),    # 1M
        today.AddDays(90),    # 3M
        today.AddDays(180),   # 6M
        today.AddDays(365),   # 12M
        today.AddDays(730),   # 24M
        today.AddDays(1095),  # 36M
        today.AddDays(1825),  # 60M
        today.AddDays(2555),  # 84M
        today.AddDays(3650),  # 120M
    ]
    fwd_rates = [rate] * len(knot_dates)
    return dal.DiscountPWLF_New(name, ccy, knot_dates, fwd_rates)


def main():
    dal.EvaluationDate_Set(dal.Date_(2024, 1, 15))
    today = dal.Date_(2024, 1, 15)
    ccy = "USD"

    # ------------------------------------------------------------------
    # Build protocol conventions
    # ------------------------------------------------------------------
    fixed_leg = dal.RateLegConvention_New(
        dal.PeriodLength_("6M"), dal.DayBasis_("ACT_360"))
    float_leg = dal.RateLegConvention_New(
        dal.PeriodLength_("6M"), dal.DayBasis_("ACT_360"))

    overnight_index = dal.RateIndexConvention_New(
        dal.PeriodLength_("1M"), dal.DayBasis_("ACT_360"),
        dal.CollateralType_OIS(), False)

    overnight_leg = dal.RateLegConvention_New(
        dal.PeriodLength_("12M"), dal.DayBasis_("ACT_360"))

    libor3m_index = dal.RateIndexConvention_New(
        dal.PeriodLength_("3M"), dal.DayBasis_("ACT_360"),
        dal.CollateralType_OIS(), True)

    # ------------------------------------------------------------------
    # Synthetic flat market (the "true" curves that instruments quote from)
    # ------------------------------------------------------------------
    ois_flat = make_flat_discount_curve("ois_market", ccy, today, 0.01)
    libor3m_flat = make_flat_discount_curve("libor3m_market", ccy, today, 0.03)

    print("=" * 72)
    print("  Exactly-Determined (Square) Multi-Curve Calibration")
    print("=" * 72)
    print()
    print(f"  Today:            {today}")
    print(f"  Currency:         {ccy}")
    print(f"  OIS market rate:  1.00%  (all instruments)")
    print(f"  Libor 3M rate:    3.00%  (all instruments)")
    print()

    # ------------------------------------------------------------------
    # Stage 1: OIS discount curve — 9 instruments on 9 knots (square)
    # ------------------------------------------------------------------
    # Each knot pillar is "pinned" by exactly one instrument.
    # The EXACT solver interpolates: zero residual at every quote.
    ois_instruments = [
        dal.Deposit_New(today, today, today.AddDays(30),  0.01, overnight_index),
        dal.Deposit_New(today, today, today.AddDays(90),  0.01, overnight_index),
        dal.Deposit_New(today, today, today.AddDays(180), 0.01, overnight_index),
        dal.OISSwap_New(today, today, today.AddDays(365),  0.01, fixed_leg, overnight_index, overnight_leg),
        dal.OISSwap_New(today, today, today.AddDays(730),  0.01, fixed_leg, overnight_index, overnight_leg),
        dal.OISSwap_New(today, today, today.AddDays(1095), 0.01, fixed_leg, overnight_index, overnight_leg),
        dal.OISSwap_New(today, today, today.AddDays(1825), 0.01, fixed_leg, overnight_index, overnight_leg),
        dal.OISSwap_New(today, today, today.AddDays(2555), 0.01, fixed_leg, overnight_index, overnight_leg),
        dal.OISSwap_New(today, today, today.AddDays(3650), 0.01, fixed_leg, overnight_index, overnight_leg),
    ]

    ois_knots = [
        today.AddDays(30),    # 1M  — pinned by deposit
        today.AddDays(90),    # 3M  — pinned by deposit
        today.AddDays(180),   # 6M  — pinned by deposit
        today.AddDays(365),   # 12M — pinned by OIS swap
        today.AddDays(730),   # 24M — pinned by OIS swap
        today.AddDays(1095),  # 36M — pinned by OIS swap
        today.AddDays(1825),  # 60M — pinned by OIS swap
        today.AddDays(2555),  # 84M — pinned by OIS swap
        today.AddDays(3650),  # 120M— pinned by OIS swap
    ]
    n_free = len(ois_knots)

    ois_builder = dal.CurveCalibrationSpecBuilder_()
    ois_builder.today_ = today
    ois_builder.ccy_ = S(ccy)
    ois_builder.curveName_ = S("ois")
    ois_builder.targetCollateral_ = dal.CollateralType_OIS()
    ois_builder.calibrateDiscountCurve_ = True
    ois_builder.solveMode_ = dal.CurveSolveMode.EXACT
    ois_builder.instruments_ = ois_instruments
    ois_builder.knotDates_ = ois_knots
    ois_spec = ois_builder.Build()

    n_ois = len(ois_instruments)
    print(f"  --- Stage 1: OIS discount curve ---")
    print(f"  Instruments:      {n_ois}")
    print(f"  Knot pillars:     {n_free}")
    print(f"  Degrees of freedom:   {n_ois - n_free}  (square system)")
    print(f"  Solver:           EXACT (interpolation)")
    print()

    # ------------------------------------------------------------------
    # Stage 2: Libor 3M forward curve — 9 instruments on 9 knots (square)
    # ------------------------------------------------------------------
    libor_instruments = [
        dal.FRA_New(today, today.AddDays(30),  today.AddDays(120),  0.03, libor3m_index),
        dal.FRA_New(today, today.AddDays(90),  today.AddDays(180),  0.03, libor3m_index),
        dal.FRA_New(today, today.AddDays(180), today.AddDays(270),  0.03, libor3m_index),
        dal.Swap_New(today, today, today.AddDays(365),  0.03, fixed_leg, libor3m_index, float_leg),
        dal.Swap_New(today, today, today.AddDays(730),  0.03, fixed_leg, libor3m_index, float_leg),
        dal.Swap_New(today, today, today.AddDays(1095), 0.03, fixed_leg, libor3m_index, float_leg),
        dal.Swap_New(today, today, today.AddDays(1825), 0.03, fixed_leg, libor3m_index, float_leg),
        dal.Swap_New(today, today, today.AddDays(2555), 0.03, fixed_leg, libor3m_index, float_leg),
        dal.Swap_New(today, today, today.AddDays(3650), 0.03, fixed_leg, libor3m_index, float_leg),
    ]

    libor_knots = [
        today.AddDays(30),    # 1M
        today.AddDays(90),    # 3M
        today.AddDays(180),   # 6M
        today.AddDays(365),   # 12M
        today.AddDays(730),   # 24M
        today.AddDays(1095),  # 36M
        today.AddDays(1825),  # 60M
        today.AddDays(2555),  # 84M
        today.AddDays(3650),  # 120M
    ]

    libor_builder = dal.CurveCalibrationSpecBuilder_()
    libor_builder.today_ = today
    libor_builder.ccy_ = S(ccy)
    libor_builder.curveName_ = S("libor3m")
    libor_builder.targetCollateral_ = dal.CollateralType_OIS()
    libor_builder.targetTenor_ = dal.PeriodLength_("3M")
    libor_builder.calibrateDiscountCurve_ = False
    libor_builder.solveMode_ = dal.CurveSolveMode.EXACT
    libor_builder.discountCurves_ = {dal.CollateralType_OIS(): ois_flat}
    libor_builder.instruments_ = libor_instruments
    libor_builder.knotDates_ = libor_knots
    libor_spec = libor_builder.Build()

    n_libor = len(libor_instruments)
    print(f"  --- Stage 2: Libor 3M forward curve ---")
    print(f"  Instruments:      {n_libor}")
    print(f"  Knot pillars:     {n_free}")
    print(f"  Degrees of freedom:   {n_libor - n_free}  (square system)")
    print(f"  Solver:           EXACT (interpolation)")
    print()

    # ------------------------------------------------------------------
    # Run sequential multi-curve calibration
    # ------------------------------------------------------------------
    multi_spec = dal.MultiCurveCalibrationSpec_()
    multi_spec.name_ = S(ccy + "_example")
    multi_spec.ccy_ = S(ccy)
    multi_spec.liborBasis_ = dal.DayBasis_("ACT_360")
    multi_spec.stages_ = [ois_spec, libor_spec]

    result = dal.CalibrateMultiCurveBundle(multi_spec)

    # ------------------------------------------------------------------
    # Print per-instrument residuals
    # ------------------------------------------------------------------
    ois_names = [
        "OIS Dep 1M",  "OIS Dep 3M",  "OIS Dep 6M",
        "OIS Swp 12M", "OIS Swp 24M", "OIS Swp 36M",
        "OIS Swp 60M", "OIS Swp 84M", "OIS Swp 120M",
    ]
    libor_names = [
        "FRA 1x4",    "FRA 3x6",   "FRA 6x9",
        "IRS  12M",   "IRS  24M",  "IRS  36M",
        "IRS  60M",   "IRS  84M",  "IRS 120M",
    ]

    for stage_idx, (names, diag) in enumerate([
        (ois_names, result.diagnostics_[0]),
        (libor_names, result.diagnostics_[1]),
    ]):
        stage_label = "OIS discount" if stage_idx == 0 else "Libor 3M forward"
        print("=" * 72)
        print(f"  Stage {stage_idx + 1}: {stage_label}  "
              f"({len(diag.marketRates_)} instruments, {n_free} free params, square)")
        print("=" * 72)
        print(f"  {'Instrument':<18} {'Market(%)':>10} {'Model(%)':>10} {'Error(bp)':>10}")
        print(f"  {'-' * 48}")
        for i in range(len(diag.marketRates_)):
            print(f"  {names[i]:<18} {diag.marketRates_[i] * 100:>10.6f} "
                  f"{diag.modelRates_[i] * 100:>10.6f} {diag.residuals_[i] * 10000:>10.4f}")

    print()
    print(f"  --- Summary ---")
    print(f"  Max abs residual (OIS):    {result.diagnostics_[0].maxAbsResidual_ * 10000:.6f} bp")
    print(f"  Max abs residual (Libor):  {result.diagnostics_[1].maxAbsResidual_ * 10000:.6f} bp")
    print(f"  RMS residual (OIS):        {result.diagnostics_[0].rmsResidual_ * 10000:.6f} bp")
    print(f"  RMS residual (Libor):      {result.diagnostics_[1].rmsResidual_ * 10000:.6f} bp")
    print()
    print(f"  Interpretation")
    print(f"  --------------")
    print(f"  Each stage is a square system: n_instruments == n_free_params.")
    print(f"  The EXACT solver interpolates directly through every quote.")
    print(f"  Residuals should be at machine-epsilon level (not curve error).")
    print(f"  This is the classic 'bootstrapping' approach — one pillar, one quote.")
    print()


if __name__ == "__main__":
    main()

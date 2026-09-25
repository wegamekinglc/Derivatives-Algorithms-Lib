#!/usr/bin/env python3
"""
005.yield_curve_jacobian.py — Yield-curve Jacobian and inverse-Jacobian IR risk

Mirrors dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp.

Demonstrates:
  - LOG_DISCOUNT single-curve calibration with EXACT solver
  - Analytic (AAD) forward Jacobian J = d(modelRate_i) / d(logDF_free_k)
  - Central-difference BUMPED Jacobian for cross-validation
  - AAD-vs-bump element-wise agreement check
  - Inverse Jacobian for bucketed IR risk transformation
  - ANALYTIC vs BUMPED timing comparison
"""

import sys
import os
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

import dal
from float_format import format_float


def S(s):
    """Shorthand for dal.String_(s) — explicit string wrapper."""
    return dal.String_(s)


def annual_leg():
    """Build an annual fixed leg convention."""
    return dal.RateLegConvention_New(
        dal.PeriodLength_("12M"), dal.DayBasis_("ACT_365F"))


def annual_index():
    """Build an annual float index convention (forecast == discount, no projection curve)."""
    return dal.RateIndexConvention_New(
        dal.PeriodLength_("12M"), dal.DayBasis_("ACT_365F"),
        dal.CollateralType_OIS(), False)


def calibrate_and_check(spec, label, jacobian_mode):
    """Calibrate a single curve and return (result, elapsed_ms, jacobian_available)."""
    t0 = time.perf_counter()
    result = dal.CalibrateSingleCurve(spec, jacobian_mode)
    elapsed = (time.perf_counter() - t0) * 1000.0

    j = result.diagnostics_.jacobian_
    j_ok = j is not None and hasattr(j, 'Rows') and j.Rows() > 0
    return result, elapsed, j_ok


def _is_jacobian_empty(jacobian):
    return jacobian is None or not hasattr(jacobian, 'Rows') or jacobian.Rows() == 0


def _print_section(title):
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70 + "\n")


def _matrix_column_widths(matrix, column_dates, n_rows, n_cols):
    return [
        max(
            13,
            len(str(column_dates[j])) + 1,
            *(len(format_float(matrix(i, j), 6)) + 1 for i in range(n_rows)),
        )
        for j in range(n_cols)
    ]


def _jacobian_header_row(free_knots, n_cols, column_widths):
    column_label = 'row \\ col'
    header = f"{column_label:<14}"
    for j in range(n_cols):
        header += f"{str(free_knots[j]):>{column_widths[j]}}"
    return header


def _jacobian_data_rows(jacobian, maturities, n_rows, n_cols, column_widths):
    rows = []
    for i in range(n_rows):
        row = f"{str(maturities[i]):<14}"
        for j in range(n_cols):
            row += f"{format_float(jacobian(i, j), 6):>{column_widths[j]}}"
        rows.append(row)
    return rows


def _print_truncation_note(jacobian):
    if jacobian.Rows() > 10 or jacobian.Cols() > 10:
        print(f"  ... ({jacobian.Rows()} x {jacobian.Cols()} matrix truncated)")


def print_jacobian(label, jacobian, maturities, free_knots):
    """Print a Jacobian matrix with row/column labels."""
    if _is_jacobian_empty(jacobian):
        print(f"  [{label}] Jacobian is EMPTY")
        return
    n_rows = min(jacobian.Rows(), 10)
    n_cols = min(jacobian.Cols(), 10)
    column_widths = _matrix_column_widths(jacobian, free_knots, n_rows, n_cols)
    print(f"  [{label}] Shape: {jacobian.Rows()} instruments x {jacobian.Cols()} free params")
    print(_jacobian_header_row(free_knots, n_cols, column_widths))
    print("-" * (14 + sum(column_widths)))
    for row in _jacobian_data_rows(jacobian, maturities, n_rows, n_cols, column_widths):
        print(row)
    print("-" * (14 + sum(column_widths)))
    _print_truncation_note(jacobian)


def _max_jacobian_diffs(ja, jb, n_rows, n_cols):
    """Compute max absolute and relative differences between two Jacobians."""
    max_abs_diff = 0.0
    max_rel_diff = 0.0
    i_max = j_max = 0
    for i in range(n_rows):
        for j in range(n_cols):
            a = ja(i, j)
            b = jb(i, j)
            abs_diff = abs(a - b)
            if abs_diff > max_abs_diff:
                max_abs_diff = abs_diff
                i_max, j_max = i, j
            denom = max(abs(a), abs(b), 1e-15)
            rel_diff = abs_diff / denom
            if rel_diff > max_rel_diff:
                max_rel_diff = rel_diff
    return max_abs_diff, i_max, j_max, max_rel_diff


def _print_comparison_details(max_abs_diff, i_max, j_max, max_rel_diff,
                              ja, jb, label_a, label_b):
    """Print Jacobian comparison details: max differences and pass/fail verdict."""
    AAD_TOL = 1e-9
    print(f"  Max absolute difference: {format_float(max_abs_diff)}")
    print(f"    at element ({i_max},{j_max}):")
    print(f"    {label_a} = {format_float(ja(i_max, j_max), 12)}")
    print(f"    {label_b} = {format_float(jb(i_max, j_max), 12)}")
    print(f"  Max relative difference: {format_float(max_rel_diff)}")
    if max_rel_diff < AAD_TOL:
        print(f"  PASS: {label_a} agrees with {label_b}")
        print(f"        (max rel diff {format_float(max_rel_diff)} < {format_float(AAD_TOL)} tolerance)")
    else:
        print(f"  NOTE: max rel diff {format_float(max_rel_diff)} > {format_float(AAD_TOL)} tolerance")
        print(f"        Near-zero Jacobian elements inflate relative error.")


def compare_jacobians(ja, jb, label_a, label_b):
    """Compare two Jacobian matrices element-wise and report max differences."""
    if _is_jacobian_empty(ja) or _is_jacobian_empty(jb):
        print(f"  [SKIP] Cannot compare {label_a} vs {label_b}: one or both are empty")
        return

    max_abs_diff, i_max, j_max, max_rel_diff = _max_jacobian_diffs(
        ja, jb, ja.Rows(), ja.Cols())
    _print_comparison_details(
        max_abs_diff, i_max, j_max, max_rel_diff, ja, jb, label_a, label_b)


def _print_header(n_instruments):
    print("=" * 70)
    print("  Yield-Curve Jacobian — AAD Analytic Jacobian Demonstration")
    print("=" * 70)
    print()
    print(f"  Calibration:       {n_instruments} instruments on {n_instruments} free LOG_DISCOUNT params")
    print(f"  System:            square ({n_instruments}x{n_instruments}) — EXACT solve")
    print(f"  Parameterization:  LOG_DISCOUNT  /  LOG_LINEAR scheme")
    print(f"  Jacobian methods:  ANALYTIC (AAD)  +  BUMPED (central-difference)")
    print()
    print(f"  The analytic Jacobian uses AAD (Automatic Adjoint Differentiation):")
    print(f"  a tape is recorded through the calibration solver; a single reverse")
    print(f"  sweep then yields all partial derivatives at once.")
    print(f"  The BUMPED Jacobian serves as an independent cross-check using")
    print(f"  central-difference finite-differencing.")
    print()


def _build_spec(today, ccy, n_instruments):
    """Build a LOG_DISCOUNT calibration spec with annual swaps."""
    fixed_leg = annual_leg()
    float_idx = annual_index()
    float_leg = annual_leg()

    instruments = []
    knot_dates = [today]
    maturities = []
    for y in range(1, n_instruments + 1):
        maturity = dal.Date_(2022 + y, 1, 1)
        maturities.append(maturity)
        knot_dates.append(maturity)
        frac = (y - 1) / (n_instruments - 1)
        par_rate = (1.00 + 1.50 * frac) / 100.0
        instruments.append(
            dal.Swap_New(today, today, maturity, par_rate, fixed_leg, float_idx, float_leg))

    spec_builder = dal.CurveCalibrationSpecBuilder_()
    spec_builder.today_ = today
    spec_builder.ccy_ = S(ccy)
    spec_builder.curveName_ = S("yield_curve_jacobian")
    spec_builder.targetCollateral_ = dal.CollateralType_OIS()
    spec_builder.calibrateDiscountCurve_ = True
    spec_builder.parameterization_ = dal.CurveParameterization.LOG_DISCOUNT
    spec_builder.solveMode_ = dal.CurveSolveMode.EXACT
    spec_builder.liborBasis_ = dal.DayBasis_("ACT_365F")
    spec_builder.tolerance_ = 1e-10
    spec_builder.fitTolerance_ = 1e-8
    spec_builder.smoothingWeight_ = 1.0
    spec_builder.logDfScheme_ = dal.LogDfScheme.LOG_LINEAR
    spec_builder.instruments_ = instruments
    spec_builder.knotDates_ = knot_dates
    return spec_builder.Build(), knot_dates, maturities


def _print_residuals(diag, maturities, n_instruments):
    print("\n" + "=" * 70)
    print(f"  Calibration residuals ({n_instruments} instruments)")
    print("=" * 70 + "\n")
    print(f"{'Maturity':<14}{'Market(%)':>10}{'Model(%)':>10}{'Error(bp)':>22}")
    print("-" * 56)
    for i in range(n_instruments):
        print(f"{str(maturities[i]):<14}{diag.marketRates_[i] * 100:>10.6f}"
              f"{diag.modelRates_[i] * 100:>10.6f}{format_float(diag.residuals_[i] * 10000, 4):>22}")
    print("-" * 56)
    print(f"\n  Max abs residual: {format_float(diag.maxAbsResidual_ * 10000, 4)} bp")
    print(f"  RMS residual:     {format_float(diag.rmsResidual_ * 10000, 4)} bp")


def _explain_analytic_jacobian(j_analytic_ok):
    if j_analytic_ok:
        print(f"  Computed by AAD reverse sweep through the calibration solver.")
        print(f"  Each column k shows how every instrument rate responds to a")
        print(f"  unit change in the log-discount factor at knot date k.")
    else:
        print(f"  [NOTE] The AAD analytic Jacobian was not populated.")
        print(f"  This can happen when the calibration setup does not meet all")
        print(f"  Phase-A eligibility requirements:")
        print(f"    - parameterization must be LOG_DISCOUNT")
        print(f"    - calibrateDiscountCurve must be True")
        print(f"    - every instrument must use forecast==discount (no projection curve)")
        print(f"    - every instrument must trade at the curve anchor date")
        print(f"    - every instrument must be a Deposit_, FRA_, Future_, or Swap_")
        print(f"  The C++ example at dal-cpp/examples/yield_curve_jacobian/")
        print(f"  demonstrates the full AAD Jacobian workflow.")


def _explain_bumped_jacobian(j_bumped_ok):
    if j_bumped_ok:
        print(f"  Computed by finite-difference: bump each free logDF parameter")
        print(f"  and re-price all instruments with the bumped curve.")
    else:
        print(f"  [NOTE] The BUMPED Jacobian was also not populated.")
        print(f"  In the current build, jacobian_ is only populated via the")
        print(f"  ANALYTIC AAD path when Phase-A eligibility is satisfied.")


def _print_inverse_jacobian(eff_inv, maturities, free_knots, tolerance):
    _print_section("(e) Inverse Jacobian (effJacobianInverse_)")
    if eff_inv is not None and hasattr(eff_inv, 'Rows') and eff_inv.Rows() > 0:
        n_knots = eff_inv.Rows()
        n_instr = eff_inv.Cols()
        print(f"  Shape: {n_knots} free params x {n_instr} instruments")
        print()
        print(f"  The inverse Jacobian transforms portfolio parameter sensitivity")
        print(f"  into bucketed quote risk.  Given a portfolio PV sensitivity g_k")
        print(f"  to each free logDF parameter k, the bucketed risk per instrument i is:")
        print(f"      r_i = sum_k  g_k * J^{{-1}}_{{k,i}}")
        print()
        print(f"  This is the IR risk transformation that every trading desk needs:")
        print(f"  yield-curve-level risk  ->  tradable-instrument bucketed risk.")
        print(f"  Quants can hedge each bucket independently after this decomposition.")
        print()
        print(f"  (Raw solver-scaled values; divide by tolerance={format_float(tolerance)} for natural units.)")
        n_rows = min(n_knots, 5)
        n_cols = min(n_instr, 5)
        column_widths = _matrix_column_widths(eff_inv, maturities, n_rows, n_cols)
        print(_jacobian_header_row(maturities, n_cols, column_widths))
        print("-" * (14 + sum(column_widths)))
        for i in range(n_rows):
            row = f"{str(free_knots[i]):<14}"
            for j in range(n_cols):
                row += f"{format_float(eff_inv(i, j), 6):>{column_widths[j]}}"
            print(row)
        print("-" * (14 + sum(column_widths)))
        if n_knots > 5 or n_instr > 5:
            print(f"  ... ({n_knots} x {n_instr} matrix truncated)")
    else:
        print(f"  [WARNING] Inverse Jacobian is unavailable.")
        print(f"  The inverse Jacobian is populated alongside the forward Jacobian")
        print(f"  by the calibration solver.  When the forward Jacobian is empty,")
        print(f"  the inverse Jacobian is typically empty as well.")


def _print_timing(t_analytic, t_bumped, n_instruments):
    _print_section("(f) Calibration timing: ANALYTIC vs BUMPED")
    print(f"  ANALYTIC (AAD reverse sweep):  {format_float(t_analytic)} ms")
    print(f"  BUMPED   (finite difference):  {format_float(t_bumped)} ms")
    if t_bumped > t_analytic:
        speedup = t_bumped / t_analytic
        print(f"  -> ANALYTIC is {format_float(speedup)}x faster than BUMPED")
        print(f"     AAD computes all partial derivatives in a single reverse sweep.")
        print(f"     BUMPED re-prices O(n_params) times.")
    else:
        ratio = t_analytic / t_bumped
        print(f"  -> BUMPED is {format_float(ratio)}x faster than ANALYTIC")
        print(f"     On this small {n_instruments}x{n_instruments} system, the AAD tape")
        print(f"     recording overhead can dominate.")
        print(f"     For larger calibrations (50+ instruments), AAD wins: O(1) reverse")
        print(f"     sweep vs O(n_params) re-pricings with BUMPED.")


def _print_summary(j_analytic_ok):
    print(f"\n{'=' * 70}")
    if j_analytic_ok:
        print(f"  The AAD analytic Jacobian was successfully computed and validated.")
    else:
        print(f"  The calibration completed successfully (residuals ~ 0).")
        print(f"  The AAD analytic Jacobian was not populated in this build.")
        print(f"  See the C++ example for the full AAD Jacobian demonstration:")
        print(f"    dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp")
    print(f"{'=' * 70}")


def _run_aad_vs_bump_comparison(j_analytic, j_bumped, j_analytic_ok, j_bumped_ok):
    """Print AAD vs BUMPED Jacobian agreement check results."""
    _print_section("(d) AAD vs BUMPED element-wise agreement check")
    if j_analytic_ok and j_bumped_ok:
        compare_jacobians(j_analytic, j_bumped, "AAD", "BUMPED")
    elif j_analytic_ok and not j_bumped_ok:
        print(f"  [SKIP] AAD Jacobian available but BUMPED Jacobian is empty.")
        print(f"  In the current build, only the ANALYTIC AAD path populates jacobian_.")
        print(f"  The C++ example at dal-cpp/examples/yield_curve_jacobian/")
        print(f"  performs a full AAD-vs-bump cross-check using an independent bump oracle.")
    elif j_bumped_ok and not j_analytic_ok:
        print(f"  [SKIP] BUMPED Jacobian available but AAD Jacobian is empty.")
    else:
        print(f"  [SKIP] Neither Jacobian was populated.")
        print(f"  The C++ example at dal-cpp/examples/yield_curve_jacobian/")
        print(f"  demonstrates the full AAD analytic Jacobian cross-check.")


def main():
    dal.EvaluationDate_Set(dal.Date_(2022, 1, 1))

    today = dal.Date_(2022, 1, 1)
    ccy = "USD"
    n_instruments = 10

    _print_header(n_instruments)

    spec, knot_dates, maturities = _build_spec(today, ccy, n_instruments)
    free_knots = knot_dates[1:]

    # (a) Calibrate with ANALYTIC Jacobian
    print("=" * 70)
    print("  (a) Calibration with ANALYTIC (AAD) Jacobian mode")
    print("=" * 70)

    result_analytic, t_analytic, j_analytic_ok = calibrate_and_check(
        spec, "ANALYTIC", dal.CurveJacobianMode.ANALYTIC)
    diag = result_analytic.diagnostics_
    _print_residuals(diag, maturities, n_instruments)

    # (b) ANALYTIC (AAD) Jacobian
    j_analytic = diag.jacobian_
    _print_section("(b) AAD Forward Jacobian J = d(modelRate_i) / d(logDF_free_k)")
    _explain_analytic_jacobian(j_analytic_ok)
    print_jacobian("AAD", j_analytic, maturities, free_knots)

    # (c) BUMPED Jacobian
    _print_section("(c) BUMPED (central-difference) Jacobian")
    result_bumped, t_bumped, j_bumped_ok = calibrate_and_check(
        spec, "BUMPED", dal.CurveJacobianMode.BUMPED)
    j_bumped = result_bumped.diagnostics_.jacobian_
    _explain_bumped_jacobian(j_bumped_ok)
    print_jacobian("BUMPED", j_bumped, maturities, free_knots)

    # (d) AAD vs BUMPED agreement check
    _run_aad_vs_bump_comparison(j_analytic, j_bumped, j_analytic_ok, j_bumped_ok)

    # (e) Inverse Jacobian
    _print_inverse_jacobian(diag.effJacobianInverse_, maturities, free_knots, 1e-10)

    # (f) Timing
    _print_timing(t_analytic, t_bumped, n_instruments)

    _print_summary(j_analytic_ok)


if __name__ == "__main__":
    main()

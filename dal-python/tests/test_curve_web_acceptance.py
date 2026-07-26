"""Compiled acceptance fixtures for the Web calibration contract."""

from __future__ import annotations

import gc
import sys
import threading

import dal


def _single_spec():
    today = dal.Date_(2026, 1, 2)
    maturity = dal.Date_(2027, 1, 2)
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    builder = dal.CurveCalibrationSpecBuilder_()
    builder.today_ = today
    builder.ccy_ = dal.String_("USD")
    builder.curveName_ = dal.String_("compiled-heartbeat")
    instruments = [dal.Deposit_New(today, today, maturity, 0.04, index)]
    builder.instruments_ = instruments
    builder.knotDates_ = [maturity]
    builder.initialGuess_ = 0.02
    return today, maturity, instruments, builder.Build()


def _assert_compiled_call_releases_gil(call) -> None:
    started = threading.Event()
    ready = threading.Event()
    stopped = threading.Event()
    heartbeat_count = [0]

    def heartbeat() -> None:
        ready.set()
        assert started.wait(timeout=5.0)  # nosec B101
        while not stopped.is_set():
            heartbeat_count[0] += 1

    previous_interval = sys.getswitchinterval()
    # Prevent a Python scheduling turn between setting ``started`` and entering
    # the compiled binding.  The private one-shot native barrier then holds the
    # real call for 75 ms *after* its public binding has released the GIL.
    sys.setswitchinterval(1.0)
    try:
        heartbeat_thread = threading.Thread(target=heartbeat)
        heartbeat_thread.start()
        assert ready.wait(timeout=5.0)  # nosec B101
        dal._dal._CurveCalibrationGilBarrier_EnableForTesting(75)
        started.set()
        call()
        count_seen_on_return = heartbeat_count[0]
    finally:
        stopped.set()
        sys.setswitchinterval(previous_interval)
        heartbeat_thread.join(timeout=5.0)
    assert not heartbeat_thread.is_alive()  # nosec B101
    assert count_seen_on_return > 0  # nosec B101


def test_fix_cb1_planner_gil_compiled_heartbeat_covers_conc_05():
    """FIX-CB1-PLANNER-GIL — planner/validator/inspector/solve release GIL."""
    today, maturity, instruments, spec = _single_spec()
    index = dal.RateIndexConvention_New(
        dal.PeriodLength_New("12M"),
        dal.DayBasis_New("ACT_365F"),
        dal.CollateralType_OIS(),
    )
    candidate_dates = [dal.Date_(2027 + index, 1, 2) for index in range(100)]
    candidate_instruments = [
        dal.Deposit_New(today, today, date, 0.04, index)
        for date in candidate_dates
    ]
    calls = (
        lambda: dal.PlanCurveCalibrationKnots(
            today,
            candidate_instruments,
            candidate_dates,
            dal.CurveKnotPolicy.INPUT,
            dal.CurveParameterization.PIECEWISE_CONSTANT_FWD,
        ),
        lambda: dal.ValidateSingleCurveAnalyticEligibility(spec),
        lambda: dal.InspectCurveCalibrationExecutionIdentity(spec),
        lambda: dal.CalibrateSingleCurve(spec),
    )
    for call in calls:
        _assert_compiled_call_releases_gil(call)


def test_api_05_compiled_curve_and_base_handles_survive_parent_gc():
    """API-05 — every concrete curve owns its recursive base/result lifetime."""
    today, maturity, _instruments, spec = _single_spec()
    result = dal.CalibrateSingleCurve(spec)
    solved = result.curve_
    del result
    gc.collect()
    assert 0.0 < solved(today, maturity) <= 1.0  # nosec B101

    knots = [maturity, dal.Date_(2028, 1, 2), dal.Date_(2029, 1, 2)]
    base = dal.DiscountPWC_New("base", "USD", knots, [0.01, 0.011, 0.012])
    curves = (
        dal.DiscountPWC_New(
            "pwc",
            "USD",
            knots,
            [0.001, 0.002, 0.003],
            base,
        ),
        dal.DiscountPWLF_New(
            "pwlf",
            "USD",
            knots,
            [0.001, 0.002, 0.003],
            [0.0015, 0.0025, 0.0035],
            base,
        ),
        dal.DiscountZeroRate_New(
            "zero",
            "USD",
            today,
            knots,
            [0.001, 0.002, 0.003],
            dal.DayBasis_New("ACT_365F"),
            dal.LogDfScheme.MIXED,
            base,
        ),
        dal.DiscountLogDF_New(
            "log",
            "USD",
            [today, *knots],
            [0.0, -0.001, -0.003, -0.006],
            day_count=dal.DayBasis_New("ACT_365F"),
            log_df_scheme=dal.LogDfScheme.MIXED,
            base=base,
        ),
    )
    del base
    gc.collect()

    for curve in curves:
        assert curve.base is not None  # nosec B101
        assert 0.0 < curve(today, maturity) <= 1.0  # nosec B101

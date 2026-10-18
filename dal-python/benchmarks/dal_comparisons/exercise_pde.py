"""Discrete-exercise benchmark references through DAL's public PDE binding."""

import dal
import numpy as np

TIME_REFINEMENT = 8


def dal_date(value):
    return dal.Date_(value.year, value.month, value.day)


def positive_counts(values):
    return all(value > 0 for value in values)


def ordinary_step_count(dates, evaluation_date, interval_steps):
    intervals = tuple((end - start).days for start, end in zip((evaluation_date, *dates), dates))
    if len(interval_steps) != len(intervals) or not dates or not positive_counts(intervals):
        raise ValueError("one positive interval and step count per exercise date required")
    if not positive_counts(interval_steps):
        raise ValueError("interval step counts must be positive")
    days = (dates[-1] - evaluation_date).days
    # The native planner allocates by ACT/365 interval length. Preserve each
    # requested minimum; unequal intervals can receive additional ordinary steps.
    return max(
        (step * days + length - 1) // length
        for step, length in zip(interval_steps, intervals)
    )


def put_surface(
    exercise_dates,
    grid_points,
    interval_steps,
    vol,
    rate,
    strike=100.0,
    dividend=0.0,
    *,
    evaluation_date,
):
    dates = tuple(exercise_dates)
    ordinary_steps = TIME_REFINEMENT * ordinary_step_count(dates, evaluation_date, interval_steps)
    contract = dal.EarlyExercisePdeContract_(
        dal_date(evaluation_date), dal_date(dates[-1]), strike, "PUT", "BERMUDAN",
        exercise_dates=[dal_date(day) for day in dates],
    )
    market = dal.EarlyExercisePdeMarket_(
        strike, vol, [dal.EarlyExercisePdeCarrySegment_(dal_date(dates[-1]), rate, dividend)]
    )
    settings = dal.EarlyExercisePdeSettings_(
        grid_points=grid_points, upper=4.0 * strike, ordinary_steps=ordinary_steps,
        theta=1.0, rannacher_intervals=0, max_actual_steps=1_000_000,
        max_work_units=10_000_000_000, numeric_payload_budget_bytes=1 << 30,
        workspace_budget_bytes=1 << 29,
    )
    result = dal.EarlyExercisePdeResult_New(dal.EarlyExercisePdeRequest_(contract, market, settings=settings))
    if not result.is_success:
        raise ValueError(f"benchmark PDE failed: {result.status}; {result.failure.reason}")
    return np.asarray(result.grid), np.asarray(result.values)

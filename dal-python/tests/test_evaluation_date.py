"""Tests for the global evaluation date functions."""

import dal


def test_evaluation_date_set_and_get():
    """EvaluationDate_Set followed by Get returns the same date."""
    d = dal.Date_(2023, 6, 15)
    dal.EvaluationDate_Set(d)
    result = dal.EvaluationDate_Get()
    assert result == d  # nosec B101 - pytest assertions are intentional


def test_evaluation_date_overwrite():
    """Setting a new evaluation date replaces the previous one."""
    d1 = dal.Date_(2022, 1, 1)
    d2 = dal.Date_(2025, 12, 31)

    dal.EvaluationDate_Set(d1)
    assert dal.EvaluationDate_Get() == d1  # nosec B101 - pytest assertions are intentional

    dal.EvaluationDate_Set(d2)
    assert dal.EvaluationDate_Get() == d2  # nosec B101 - pytest assertions are intentional


def test_evaluation_date_set_same_twice():
    """Setting the same date twice is idempotent."""
    d = dal.Date_(2024, 3, 15)
    dal.EvaluationDate_Set(d)
    dal.EvaluationDate_Set(d)
    assert dal.EvaluationDate_Get() == d  # nosec B101 - pytest assertions are intentional


def test_evaluation_date_persists_across_calls():
    """Evaluation date persists across multiple Get calls."""
    d = dal.Date_(2023, 7, 4)
    dal.EvaluationDate_Set(d)

    r1 = dal.EvaluationDate_Get()
    r2 = dal.EvaluationDate_Get()
    r3 = dal.EvaluationDate_Get()

    assert r1 == d  # nosec B101 - pytest assertions are intentional
    assert r2 == d  # nosec B101 - pytest assertions are intentional
    assert r3 == d  # nosec B101 - pytest assertions are intentional


def test_evaluation_date_various_dates():
    """Evaluation date works for various date values."""
    dates = [
        dal.Date_(2000, 1, 1),
        dal.Date_(2020, 2, 29),
        dal.Date_(2030, 12, 31),
        dal.Date_(2022, 9, 25),
    ]
    for d in dates:
        dal.EvaluationDate_Set(d)
        assert dal.EvaluationDate_Get() == d  # nosec B101 - pytest assertions are intentional

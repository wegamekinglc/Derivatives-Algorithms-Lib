"""Shared fixtures for the DAL Python binding test suite."""

from pathlib import Path
import sys
from types import SimpleNamespace

import pytest
import dal

# Make dal_benchmarks/ importable without installing it (used by test_benchmarks.py).
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "benchmarks"))


@pytest.fixture(autouse=True)
def reset_evaluation_date():
    """Reset the global evaluation date before each test for isolation."""
    dal.EvaluationDate_Set(dal.Date_(2022, 9, 25))
    yield


@pytest.fixture
def historical_fix():
    """The shared SCALE x FIX scenario: 2x a historical index fixing, paid later."""
    index = "EQ[DAL196_TEST]"
    today = dal.Date_(2026, 9, 12)
    history = dal.Date_(2026, 9, 11)
    payment = dal.Date_(2026, 9, 22)

    def product():
        return dal.Product_New(
            ["SCALE", history, payment],
            ["2", f"x = SCALE * FIX({index})", "pay PAYS x"],
        )

    def snapshot(value=80.0, date=history, hour=0):
        return dal.MarketFixingSnapshot_New(
            {index: {dal.DateTime_(date, hour): value}}
        )

    return SimpleNamespace(
        today=today,
        history=history,
        payment=payment,
        index=index,
        product=product,
        snapshot=snapshot,
    )

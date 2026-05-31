"""Shared fixtures for the DAL Python binding test suite."""

import pytest
import dal


@pytest.fixture(autouse=True)
def reset_evaluation_date():
    """Reset the global evaluation date before each test for isolation."""
    dal.EvaluationDate_Set(dal.Date_(2022, 9, 25))
    yield


@pytest.fixture
def eval_date():
    """A fixed evaluation date: 2022-09-25."""
    return dal.Date_(2022, 9, 25)


@pytest.fixture
def maturity_date():
    """A maturity date roughly 3 years from eval date."""
    return dal.Date_(2025, 9, 24)


@pytest.fixture
def bs_model():
    """Black-Scholes model with standard parameters."""
    return dal.BSModelData_New(spot=100.0, vol=0.2, rate=0.05, div=0.02)


@pytest.fixture
def european_call(maturity_date):
    """A simple European call option product."""
    strike = 100.0
    dates = [dal.Cell_("STRIKE"), dal.Cell_(maturity_date)]
    events = [str(strike), "call pays MAX(spot() - STRIKE, 0.0)"]
    return dal.Product_New(dates, events)


@pytest.fixture
def bs_params():
    """Black-Scholes parameters as a plain dict for reference."""
    return {
        "spot": 100.0,
        "vol": 0.2,
        "rate": 0.05,
        "div": 0.02,
        "strike": 100.0,
    }

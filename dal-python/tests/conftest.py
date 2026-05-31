"""Shared fixtures for the DAL Python binding test suite."""

import pytest
import dal


@pytest.fixture(autouse=True)
def reset_evaluation_date():
    """Reset the global evaluation date before each test for isolation."""
    dal.EvaluationDate_Set(dal.Date_(2022, 9, 25))
    yield

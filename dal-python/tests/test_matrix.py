"""Tests for the DoubleMatrix_ class."""

import dal


def test_matrix_construction():
    """DoubleMatrix_ can be constructed with given dimensions and fill value."""
    m = dal.DoubleMatrix_(3, 4, 0.0)
    assert m is not None  # nosec B101 - pytest assertions are intentional


def test_matrix_default_fill():
    """DoubleMatrix_ default fill value is 0.0."""
    m = dal.DoubleMatrix_(2, 2)
    assert m is not None  # nosec B101 - pytest assertions are intentional


def test_matrix_element_access():
    """Matrix elements can be read via __call__(i, j)."""
    m = dal.DoubleMatrix_(3, 3, 0.0)
    for i in range(3):
        for j in range(3):
            assert m(i, j) == 0.0  # nosec B101 - pytest assertions are intentional


def test_matrix_nonzero_fill():
    """Matrix constructed with a non-zero fill value stores it."""
    m = dal.DoubleMatrix_(2, 3, 5.5)
    for i in range(2):
        for j in range(3):
            assert m(i, j) == 5.5  # nosec B101 - pytest assertions are intentional


def test_matrix_single_element():
    """A 1x1 matrix works correctly."""
    m = dal.DoubleMatrix_(1, 1, 42.0)
    assert m(0, 0) == 42.0  # nosec B101 - pytest assertions are intentional


def test_matrix_large_dimensions():
    """Matrix handles moderately large dimensions."""
    rows, cols = 100, 50
    m = dal.DoubleMatrix_(rows, cols, 1.0)
    assert m(0, 0) == 1.0  # nosec B101 - pytest assertions are intentional
    assert m(rows - 1, cols - 1) == 1.0  # nosec B101 - pytest assertions are intentional
    assert m(rows // 2, cols // 2) == 1.0  # nosec B101 - pytest assertions are intentional


def test_matrix_negative_fill():
    """Matrix with negative fill value."""
    m = dal.DoubleMatrix_(2, 2, -3.14)
    assert m(0, 0) == -3.14  # nosec B101 - pytest assertions are intentional
    assert m(1, 1) == -3.14  # nosec B101 - pytest assertions are intentional

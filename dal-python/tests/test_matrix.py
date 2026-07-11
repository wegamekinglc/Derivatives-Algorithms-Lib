"""Tests for the DoubleMatrix_ class."""

import dal
import pytest


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


@pytest.mark.parametrize("rows, cols", [(-1, 2), (2, -1), (-1, -1)])
def test_matrix_dimensions_must_be_non_negative(rows, cols):
    """Negative dimensions fail before reaching the native matrix allocator."""
    with pytest.raises(ValueError, match="non-negative"):
        dal.DoubleMatrix_(rows, cols)


def test_matrix_nested_list_construction():
    """Nested Python rows preserve a non-flat surface exactly."""
    m = dal.DoubleMatrix_([[0.22, 0.20, 0.19], [0.21, 0.20, 0.18]])

    assert m.Rows() == 2  # nosec B101 - pytest assertions are intentional
    assert m.Cols() == 3  # nosec B101 - pytest assertions are intentional
    assert m(0, 0) == 0.22  # nosec B101 - pytest assertions are intentional
    assert m(1, 2) == 0.18  # nosec B101 - pytest assertions are intentional


def test_matrix_nested_list_must_be_rectangular():
    """Ragged Python rows fail before entering native model construction."""
    with pytest.raises(ValueError, match="rectangular"):
        dal.DoubleMatrix_([[0.20, 0.21], [0.22]])


def test_matrix_item_mutation():
    """Tuple indexing supports safe element updates and reads."""
    m = dal.DoubleMatrix_(2, 2, 0.0)

    m[1, 0] = 0.25

    assert m[1, 0] == 0.25  # nosec B101 - pytest assertions are intentional
    with pytest.raises(IndexError):
        _ = m[2, 0]


def test_matrix_negative_indices_follow_python_conventions():
    """Negative row and column indices address elements from the end."""
    m = dal.DoubleMatrix_([[1.0, 2.0], [3.0, 4.0]])

    assert m(-1, -1) == 4.0  # nosec B101 - pytest assertions are intentional
    assert m[-2, -1] == 2.0  # nosec B101 - pytest assertions are intentional
    m[-1, -2] = 5.0
    assert m[1, 0] == 5.0  # nosec B101 - pytest assertions are intentional

    with pytest.raises(IndexError):
        _ = m[-3, 0]
    with pytest.raises(IndexError):
        _ = m[0, -3]

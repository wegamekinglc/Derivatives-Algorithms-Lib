"""Tests for the DoubleMatrix_ class."""

import subprocess
import sys
import textwrap

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


@pytest.mark.parametrize("rows, cols", [(-1, 2), (2, -1), (-1, -1), (-(2**100), 1), (1, -(2**100))])
def test_matrix_dimensions_must_be_non_negative(rows, cols):
    """Negative dimensions fail before reaching the native matrix allocator."""
    with pytest.raises(ValueError, match="non-negative"):
        dal.DoubleMatrix_(rows, cols)


@pytest.mark.parametrize(
    "rows, cols",
    [
        (2**63, 1),
        (1, 2**63),
        (2**100, 1),
        (1, 2**100),
    ],
)
def test_matrix_huge_dimensions_raise_value_error(rows, cols):
    """Python integers beyond native dimension bounds fail deterministically."""
    with pytest.raises(ValueError, match="dimensions|range|storage"):
        dal.DoubleMatrix_(rows, cols)


@pytest.mark.parametrize("rows, cols", [(2.0, 2), (2, 2.0), ("2", 2), (2, None)])
def test_matrix_dimensions_must_be_integers(rows, cols):
    """Objects without Python's integer index protocol are not dimensions."""
    with pytest.raises(TypeError, match="must be integers"):
        dal.DoubleMatrix_(rows, cols)


def test_matrix_boolean_dimensions_preserve_python_integer_semantics():
    """Booleans remain accepted as Python integer dimensions for compatibility."""
    m = dal.DoubleMatrix_(True, False, 1.25)

    assert (m.Rows(), m.Cols()) == (1, 0)  # nosec B101


def test_matrix_dimensions_accept_python_index_protocol():
    """Integer-like dimensions retain index-protocol and fill support."""

    class Dimension:
        def __init__(self, value):
            self.value = value

        def __index__(self):
            return self.value

    m = dal.DoubleMatrix_(Dimension(2), Dimension(3), 1.25)

    assert (m.Rows(), m.Cols()) == (2, 3)  # nosec B101
    assert m(1, 2) == 1.25  # nosec B101


@pytest.mark.parametrize(
    "rows, cols",
    [
        (2**31 - 1, 0),
        (2**31 - 1, 1),
        (2**30, 2),
        (46341, 46341),
        (0, 2**31),
    ],
)
def test_matrix_dimensions_must_fit_native_sentinel_storage(rows, cols):
    """Unsafe sentinel-row products fail before native construction."""
    with pytest.raises(ValueError, match="dimensions|storage"):
        dal.DoubleMatrix_(rows, cols)


def test_matrix_exact_sentinel_boundary_preserves_memory_error():
    """An arithmetic-valid boundary reaches allocation and maps bad_alloc correctly."""
    if sys.platform == "win32":
        pytest.skip("the resource module is unavailable on Windows")
    pytest.importorskip("resource")

    probe = textwrap.dedent(
        """
        import resource
        import sys

        import dal

        _, hard_limit = resource.getrlimit(resource.RLIMIT_AS)
        soft_limit = 256 * 1024 * 1024
        if hard_limit != resource.RLIM_INFINITY:
            soft_limit = min(soft_limit, hard_limit)
        resource.setrlimit(resource.RLIMIT_AS, (soft_limit, hard_limit))

        try:
            dal.DoubleMatrix_(0, 2**31 - 1)
        except MemoryError:
            raise SystemExit(0)
        except Exception as exc:
            print(f"unexpected {type(exc).__name__}: {exc}", file=sys.stderr)
            raise SystemExit(2)
        raise SystemExit(3)
        """
    )
    completed = subprocess.run(
        [sys.executable, "-c", probe],
        check=False,
        capture_output=True,
        text=True,
        timeout=30,
    )

    assert completed.returncode == 0, completed.stdout + completed.stderr  # nosec B101


def test_matrix_zero_sized_dimensions_remain_valid():
    """Modest matrices with a zero logical extent preserve their shape."""
    no_rows = dal.DoubleMatrix_(0, 5)
    no_cols = dal.DoubleMatrix_(5, 0)

    assert (no_rows.Rows(), no_rows.Cols()) == (0, 5)  # nosec B101
    assert (no_cols.Rows(), no_cols.Cols()) == (5, 0)  # nosec B101


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


def test_matrix_nested_rows_share_dimension_preflight():
    """A sequence advertising an unsafe row width is rejected before iteration."""

    class OversizedRow:
        def __len__(self):
            return 2**30

        def __getitem__(self, index):
            raise AssertionError(f"unsafe row was iterated at index {index}")

    with pytest.raises(ValueError, match="dimensions|storage"):
        dal.DoubleMatrix_([OversizedRow()])


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

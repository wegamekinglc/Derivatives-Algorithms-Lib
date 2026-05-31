"""Tests for the Cell_ class (polymorphic value container)."""

import dal


def test_cell_from_bool():
    """Cell_ can be constructed from a bool."""
    c = dal.Cell_(True)
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_bool_false():
    """Cell_ from False works."""
    c = dal.Cell_(False)
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_double():
    """Cell_ can be constructed from a float."""
    c = dal.Cell_(3.14)
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_double_zero():
    """Cell_ from 0.0 works."""
    c = dal.Cell_(0.0)
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_double_negative():
    """Cell_ from a negative float works."""
    c = dal.Cell_(-100.5)
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_date():
    """Cell_ can be constructed from a Date_."""
    d = dal.Date_(2022, 9, 25)
    c = dal.Cell_(d)
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_string():
    """Cell_ can be constructed from a String_."""
    s = dal.String_("STRIKE")
    c = dal.Cell_(s)
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_cstr():
    """Cell_ can be constructed from a Python string (const char*)."""
    c = dal.Cell_("BARRIER")
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cell_from_empty_string():
    """Cell_ from an empty string works."""
    c = dal.Cell_("")
    assert c is not None  # nosec B101 - pytest assertions are intentional


def test_cellvector():
    """CellVector (std::vector<Cell_>) works as a list-like container."""
    cv = dal.CellVector()
    assert len(cv) == 0  # nosec B101 - pytest assertions are intentional

    cv.append(dal.Cell_(True))
    cv.append(dal.Cell_(42.0))
    cv.append(dal.Cell_(dal.Date_(2022, 1, 1)))
    cv.append(dal.Cell_("hello"))
    assert len(cv) == 4  # nosec B101 - pytest assertions are intentional


def test_cell_vector_mixed_types():
    """CellVector can hold cells of different underlying types."""
    cv = dal.CellVector()
    cv.append(dal.Cell_(1.0))
    cv.append(dal.Cell_("STRIKE"))
    cv.append(dal.Cell_(dal.Date_(2025, 12, 31)))
    cv.append(dal.Cell_(False))
    assert len(cv) == 4  # nosec B101 - pytest assertions are intentional

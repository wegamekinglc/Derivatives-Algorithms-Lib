"""Tests for SWIG-wrapped std::vector types."""

import dal


def test_doublevector_empty():
    """DoubleVector starts empty and grows via append."""
    v = dal.DoubleVector()
    assert len(v) == 0


def test_doublevector_append_and_access():
    """DoubleVector supports append and indexed access."""
    v = dal.DoubleVector()
    values = [1.0, 2.5, -3.14, 0.0, 100.0]
    for val in values:
        v.append(val)

    assert len(v) == len(values)
    for i, expected in enumerate(values):
        assert v[i] == expected


def test_doublevector_setitem():
    """DoubleVector supports indexed assignment."""
    v = dal.DoubleVector()
    v.append(0.0)
    v.append(0.0)
    v[0] = 42.0
    v[1] = -7.5
    assert v[0] == 42.0
    assert v[1] == -7.5


def test_doublevector_large():
    """DoubleVector handles many elements."""
    v = dal.DoubleVector()
    n = 10000
    for i in range(n):
        v.append(float(i))
    assert len(v) == n
    assert v[0] == 0.0
    assert v[n - 1] == float(n - 1)


def test_strvector_empty():
    """StrVector starts empty."""
    v = dal.StrVector()
    assert len(v) == 0


def test_strvector_append_and_access():
    """StrVector supports append and indexed access."""
    v = dal.StrVector()
    items = ["alpha", "beta", "gamma", "delta"]
    for item in items:
        v.append(item)

    assert len(v) == len(items)
    for i, expected in enumerate(items):
        assert v[i] == expected


def test_datevector_append_and_access():
    """DateVector supports append and indexed access."""
    dv = dal.DateVector()
    dates = [
        dal.Date_(2022, 1, 1),
        dal.Date_(2022, 6, 15),
        dal.Date_(2023, 12, 31),
    ]
    for d in dates:
        dv.append(d)

    assert len(dv) == 3
    assert dal.Year(dv[0]) == 2022
    assert dal.Month(dv[1]) == 6
    assert dal.Day(dv[2]) == 31


def test_cellvector_append_and_access():
    """CellVector supports append and indexed access."""
    cv = dal.CellVector()
    cv.append(dal.Cell_(1.0))
    cv.append(dal.Cell_("STRIKE"))
    cv.append(dal.Cell_(dal.Date_(2025, 1, 1)))

    assert len(cv) == 3

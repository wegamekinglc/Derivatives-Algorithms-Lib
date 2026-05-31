"""Tests for the Date_ class."""

import dal


def test_date_construction():
    """Date_ can be constructed from year, month, day."""
    d = dal.Date_(2022, 9, 25)
    assert dal.Year(d) == 2022
    assert dal.Month(d) == 9
    assert dal.Day(d) == 25


def test_date_construction_various_dates():
    """Date_ works for a range of valid dates."""
    cases = [
        (2000, 1, 1),
        (1999, 12, 31),
        (2024, 2, 29),  # leap year
        (2023, 6, 15),
        (2030, 12, 31),
    ]
    for y, m, d in cases:
        dt = dal.Date_(y, m, d)
        assert dal.Year(dt) == y
        assert dal.Month(dt) == m
        assert dal.Day(dt) == d


def test_date_add_days():
    """AddDays returns a new Date_ shifted forward."""
    d = dal.Date_(2022, 9, 25)
    d2 = d.AddDays(10)
    assert dal.Year(d2) == 2022
    assert dal.Month(d2) == 10
    assert dal.Day(d2) == 5


def test_date_add_days_month_boundary():
    """AddDays correctly crosses month boundaries."""
    d = dal.Date_(2022, 1, 30)
    d2 = d.AddDays(5)
    assert dal.Month(d2) == 2
    assert dal.Day(d2) == 4


def test_date_add_days_year_boundary():
    """AddDays correctly crosses year boundaries."""
    d = dal.Date_(2022, 12, 30)
    d2 = d.AddDays(5)
    assert dal.Year(d2) == 2023
    assert dal.Month(d2) == 1
    assert dal.Day(d2) == 4


def test_date_add_days_leap_year():
    """AddDays handles leap year Feb 29 correctly."""
    d = dal.Date_(2024, 2, 27)
    d2 = d.AddDays(2)
    assert dal.Month(d2) == 2
    assert dal.Day(d2) == 29


def test_date_subtraction():
    """Subtracting two dates gives the number of days between them."""
    d1 = dal.Date_(2022, 9, 25)
    d2 = dal.Date_(2022, 10, 5)
    assert d2 - d1 == 10


def test_date_subtraction_same_date():
    """Same date subtracted gives zero."""
    d = dal.Date_(2022, 9, 25)
    assert d - d == 0


def test_date_subtraction_negative():
    """Subtracting a later date from an earlier one gives negative."""
    d1 = dal.Date_(2022, 9, 25)
    d2 = dal.Date_(2022, 10, 5)
    assert d1 - d2 == -10


def test_date_comparisons():
    """Date_ supports all comparison operators."""
    d1 = dal.Date_(2022, 9, 25)
    d2 = dal.Date_(2022, 10, 5)
    d3 = dal.Date_(2022, 9, 25)

    assert d1 < d2
    assert d1 <= d2
    assert d1 <= d3
    assert d2 > d1
    assert d2 >= d1
    assert d3 >= d1
    assert d1 == d3
    assert not (d1 == d2)
    assert not (d1 < d3)
    assert not (d2 < d1)


def test_date_repr():
    """Date_ has a readable string representation."""
    d = dal.Date_(2022, 9, 25)
    r = repr(d)
    assert "2022" in r
    assert "9" in r or "Sep" in r or "09" in r
    assert "25" in r


def test_datevector():
    """DateVector (std::vector<Date_>) works as a list-like container."""
    dv = dal.DateVector()
    assert len(dv) == 0

    dv.append(dal.Date_(2022, 1, 1))
    dv.append(dal.Date_(2022, 6, 15))
    dv.append(dal.Date_(2023, 12, 31))
    assert len(dv) == 3
    assert dal.Year(dv[0]) == 2022
    assert dal.Month(dv[1]) == 6
    assert dal.Day(dv[2]) == 31

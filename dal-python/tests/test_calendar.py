"""Tests for China calendar types (Holidays_, BizDayConvention_, CountBusDays_)."""

import dal


# ---- Holidays_ construction ----

def test_holidays_construct_cn_sse():
    """Holidays_ can be constructed for CN.SSE (Shanghai Stock Exchange)."""
    hol = dal.Holidays_("CN.SSE")
    assert hol is not None  # nosec B101 - pytest assertions are intentional
    assert str(hol) == "Holidays_(\"CN.SSE\")"  # nosec B101 - pytest assertions are intentional


def test_holidays_construct_cn_ib():
    """Holidays_ can be constructed for CN.IB (China Interbank)."""
    hol = dal.Holidays_("CN.IB")
    assert hol is not None  # nosec B101 - pytest assertions are intentional
    assert "CN.IB" in str(hol)  # nosec B101 - pytest assertions are intentional


def test_holidays_construct_composite():
    """Holidays_ can be constructed from multiple centers."""
    hol = dal.Holidays_("CN.SSE CN.IB")
    assert hol is not None  # nosec B101 - pytest assertions are intentional
    r = str(hol)
    assert "CN.IB" in r  # nosec B101 - pytest assertions are intentional
    assert "CN.SSE" in r  # nosec B101 - pytest assertions are intentional


# ---- IsHoliday — China SSE ----

def test_cn_sse_is_holiday_national_day():
    """CN.SSE: National Day (Oct 1-7) are holidays (weekdays only; weekends excluded from list)."""
    hol = dal.Holidays_("CN.SSE")
    # National Day golden week 2020 — Oct 1-2, 5-7 are weekdays
    for d in [1, 2, 5, 6, 7]:
        assert hol.IsHoliday(dal.Date_(2020, 10, d))  # nosec B101 - pytest assertions are intentional


def test_cn_sse_is_holiday_spring_festival():
    """CN.SSE: Chinese New Year / Spring Festival weekdays are holidays."""
    hol = dal.Holidays_("CN.SSE")
    # Spring Festival 2024 weekdays: Feb 9 (Fri), Feb 12-16 (Mon-Fri)
    for d in [9, 12, 13, 14, 15, 16]:
        assert hol.IsHoliday(dal.Date_(2024, 2, d))  # nosec B101 - pytest assertions are intentional


def test_cn_sse_not_holiday_regular_day():
    """CN.SSE: A regular business day is not a holiday."""
    hol = dal.Holidays_("CN.SSE")
    # March 15, 2024 is a Friday (not a holiday)
    assert not hol.IsHoliday(dal.Date_(2024, 3, 15))  # nosec B101 - pytest assertions are intentional


# ---- IsHoliday — China IB ----

def test_cn_ib_is_holiday_same_as_sse():
    """CN.IB shares most holidays with CN.SSE (National Day)."""
    hol = dal.Holidays_("CN.IB")
    assert hol.IsHoliday(dal.Date_(2020, 10, 1))  # nosec B101 - pytest assertions are intentional
    assert hol.IsHoliday(dal.Date_(2020, 10, 7))  # nosec B101 - pytest assertions are intentional


def test_cn_ib_no_special_workday():
    """CN.IB: Feb 9 2024 is a special SSE workday, removed from IB holidays."""
    hol_sse = dal.Holidays_("CN.SSE")
    hol_ib = dal.Holidays_("CN.IB")
    d = dal.Date_(2024, 2, 9)
    # For CN.IB, Feb 9 2024 is NOT a holiday (it's a special working day)
    # For CN.SSE, it IS a holiday
    assert hol_sse.IsHoliday(d)  # nosec B101 - pytest assertions are intentional
    assert not hol_ib.IsHoliday(d)  # nosec B101 - pytest assertions are intentional


# ---- IsWorkWeekends ----

def test_cn_ib_work_weekends():
    """CN.IB: Known working weekends are marked as work weekends."""
    hol = dal.Holidays_("CN.IB")
    # Oct 10, 2020 (Saturday) is a working weekend to compensate for National Day
    assert hol.IsWorkWeekends(dal.Date_(2020, 10, 10))  # nosec B101 - pytest assertions are intentional
    # Sep 27, 2020 (Sunday) is a working weekend
    assert hol.IsWorkWeekends(dal.Date_(2020, 9, 27))  # nosec B101 - pytest assertions are intentional


def test_cn_sse_no_work_weekends():
    """CN.SSE: SSE does not have working weekends."""
    hol = dal.Holidays_("CN.SSE")
    assert not hol.IsWorkWeekends(dal.Date_(2020, 10, 10))  # nosec B101 - pytest assertions are intentional


def test_composite_has_work_weekends():
    """Composite 'CN.SSE CN.IB' inherits work weekends from CN.IB."""
    hol = dal.Holidays_("CN.SSE CN.IB")
    assert hol.IsWorkWeekends(dal.Date_(2020, 10, 10))  # nosec B101 - pytest assertions are intentional


# ---- IsBusinessDay ----

def test_is_business_day_cn_sse():
    """IsBusinessDay returns True for regular business days, False for holidays."""
    hol = dal.Holidays_("CN.SSE")
    # Oct 9, 2020 is a Friday between National Day and a weekend — it's a business day
    assert dal.IsBusinessDay(hol, dal.Date_(2020, 10, 9))  # nosec B101 - pytest assertions are intentional
    # Oct 7, 2020 is National Day holiday
    assert not dal.IsBusinessDay(hol, dal.Date_(2020, 10, 7))  # nosec B101 - pytest assertions are intentional


def test_is_business_day_cn_ib():
    """IsBusinessDay CN.IB: working weekends count as business days."""
    hol = dal.Holidays_("CN.IB")
    # Oct 10, 2020 is a Saturday but a working weekend
    assert dal.IsBusinessDay(hol, dal.Date_(2020, 10, 10))  # nosec B101 - pytest assertions are intentional


# ---- NextBus / PrevBus ----

def test_next_bus_cn_sse():
    """NextBus finds the next business day after a Saturday."""
    hol = dal.Holidays_("CN.SSE")
    # Oct 10, 2020 is Saturday. Next business day is Monday Oct 12
    d = dal.Date_(2020, 10, 10)
    nxt = dal.NextBus(hol, d)
    assert nxt == dal.Date_(2020, 10, 12)  # nosec B101 - pytest assertions are intentional


def test_next_bus_cn_ib_work_weekend():
    """NextBus CN.IB: a working weekend is its own next business day."""
    hol = dal.Holidays_("CN.IB")
    d = dal.Date_(2020, 10, 10)
    nxt = dal.NextBus(hol, d)
    assert nxt == dal.Date_(2020, 10, 10)  # nosec B101 - pytest assertions are intentional


def test_prev_bus_cn_sse():
    """PrevBus finds the previous business day before a Saturday."""
    hol = dal.Holidays_("CN.SSE")
    # Oct 10, 2020 is Saturday. Previous business day is Friday Oct 9
    d = dal.Date_(2020, 10, 10)
    prv = dal.PrevBus(hol, d)
    assert prv == dal.Date_(2020, 10, 9)  # nosec B101 - pytest assertions are intentional


def test_prev_bus_cn_ib_work_weekend():
    """PrevBus CN.IB: a working weekend is its own previous business day."""
    hol = dal.Holidays_("CN.IB")
    d = dal.Date_(2020, 10, 10)
    prv = dal.PrevBus(hol, d)
    assert prv == dal.Date_(2020, 10, 10)  # nosec B101 - pytest assertions are intentional


# ---- Adjust ----

def test_adjust_following_cn_sse():
    """Adjust with FOLLOWING convention: a holiday rolls to next business day."""
    hol = dal.Holidays_("CN.SSE")
    # Oct 5, 2020 is National Day holiday. Following -> Oct 9 (next bus day)
    d = dal.Date_(2020, 10, 5)
    adj = dal.Adjust(hol, d, dal.BizDayConvention_.FOLLOWING)
    assert adj == dal.Date_(2020, 10, 9)  # nosec B101 - pytest assertions are intentional


def test_adjust_unadjusted():
    """Adjust with UNADJUSTED convention: date stays the same."""
    hol = dal.Holidays_("CN.SSE")
    d = dal.Date_(2020, 10, 5)
    adj = dal.Adjust(hol, d, dal.BizDayConvention_.UNADJUSTED)
    assert adj == d  # nosec B101 - pytest assertions are intentional


# ---- CountBusDays_ ----

def test_count_bus_days_cn_sse():
    """CountBusDays_ counts business days between dates with CN.SSE calendar."""
    hol = dal.Holidays_("CN.SSE")
    counter = dal.CountBusDays_(hol)
    # Oct 9 (Fri) to Oct 11 (Sun): only 1 business day (Fri)
    start = dal.Date_(2020, 10, 9)
    end = dal.Date_(2020, 10, 11)
    assert counter(start, end) == 1  # nosec B101 - pytest assertions are intentional


def test_count_bus_days_cn_ib():
    """CountBusDays_ CN.IB counts working weekends as business days."""
    hol = dal.Holidays_("CN.IB")
    counter = dal.CountBusDays_(hol)
    # Oct 9 (Fri) to Oct 11 (Sun): Oct 10 is IB working weekend, so 2 bus days
    start = dal.Date_(2020, 10, 9)
    end = dal.Date_(2020, 10, 11)
    assert counter(start, end) == 2  # nosec B101 - pytest assertions are intentional


# ---- BizDayConvention_ enum ----

def test_biz_day_convention_values():
    """BizDayConvention_ has expected values."""
    assert dal.BizDayConvention_.UNADJUSTED is not None  # nosec B101 - pytest assertions are intentional
    assert dal.BizDayConvention_.FOLLOWING is not None  # nosec B101 - pytest assertions are intentional
    assert dal.BizDayConvention_.MODIFIED_FOLLOWING is not None  # nosec B101 - pytest assertions are intentional


# ---- Cross-calendar: CN.SSE vs CN.IB composite ----

def test_composite_next_bus():
    """Composite calendar: NextBus treats IB working weekends as business days."""
    hol = dal.Holidays_("CN.SSE CN.IB")
    # Oct 10, 2020: CN.SSE says weekend, CN.IB says work weekend — composite says bus day
    d = dal.Date_(2020, 10, 10)
    nxt = dal.NextBus(hol, d)
    assert nxt == dal.Date_(2020, 10, 10)  # nosec B101 - pytest assertions are intentional

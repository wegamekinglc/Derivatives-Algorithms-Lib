# Dates, Calendars, and Schedules

This note describes the date machinery in `dal-cpp/dal/time/`: the `Date_`
value type, holiday centers and business-day adjustment, date increments,
schedule generation, and day-count bases.

## Dates

`Date_` (`dal-cpp/dal/time/date.hpp`) is a value type wrapping a `uint16_t`
serial day count. Construction from year, month, and day validates each field
and accepts years in [1900, 2199]; a default-constructed `Date_` is invalid
(`IsValid()` returns false). Comparisons and `operator-` (a day difference)
work directly on the serial, and `AddDays`, `++`, and `--` shift by whole
days.

Free functions in `namespace Dal::Date` cover component access (`Year`,
`Month`, `Day`, `DayOfWeek`), month arithmetic (`AddMonths` with optional
end-of-month preservation, `EndOfMonth`), and Excel interop: `FromExcel` /
`ToExcel` convert to and from the Excel 1900 date system, and
`NumericValueOf` exposes the Excel serial as a `double` for storage and
interpolation coordinates. `Date::FromString`
(`dal-cpp/dal/time/dateutils.cpp`) recognizes `mm/dd/yyyy` (two-digit years
map to 20xx) and `yyyy-mm-dd`; `Date::IsDateString` predicts whether a string
matches one of those formats.

`DateTime_` (`dal-cpp/dal/time/datetime.hpp`) pairs a `Date_` with a `double`
day fraction, giving roughly one-second resolution for fixing times.

## Holiday centers and business days

Holiday data lives in process-wide named centers
(`dal-cpp/dal/time/holidaydata.hpp`). Each `HolidayCenterData_` holds a center
name, a sorted holiday list, and a sorted work-weekend list (weekend days
that count as business days). `Holidays::AddCenter` registers a center;
`Calendars_::Init` (`dal-cpp/dal/time/calendars/init.cpp`) registers the
built-in centers — `CN.SSE`, `CN.IB` (which adds work weekends), and `TARGET`
(computed over 2000-2100) — when `RegisterAll_::Init` runs at library
initialization.

A `Holidays_` object (`dal-cpp/dal/time/holidays.hpp`) is built from a
space-separated list of center names and unions their calendars; the empty
string selects no holidays (`Holidays::None()`). A date is a business day
when it is a listed work weekend, or a weekday that is not a holiday in any
constituent center (`Holidays::IsBusinessDay`). `Holidays::NextBus` and
`PrevBus` roll a date to the nearest business day, and `Holidays::Adjust`
applies a `BizDayConvention_` — `Unadjusted`, `Following`,
`ModifiedFollowing`, or `Preceding`.

`CountBusDays_` counts business days in a range. Multi-center combinations
are merged once into a cached combined center; counting then handles full
weeks arithmetically and scans only the partial week at the end of the range.

## Date increments

`Date::Increment_` (`dal-cpp/dal/time/dateincrement.hpp`) is the step
interface: `FwdFrom` and `BackFrom` move a date forward or backward.
`Date::ParseIncrement` builds increments from strings:

- tenor steps such as `3M`, `10Y`, or `2W`, with step sizes from the
  `DateStepSize_` enumeration (`Y`, `M`, `W`, `BD`, `CD`);
- an optional holiday suffix after `;` (for example `5BD;CN.IB`) that rolls
  the stepped date to a business day on those calendars;
- special-day names (`IMM`, `IMM1`, `CDS`, `EOM`) that step to the next or
  previous quarterly or monthly IMM date, CDS standard maturity, or month
  end; and
- `&`-joined compounds that apply each sub-increment in sequence.

`Date::NBusDays` and `Date::ToIMM` are factory shortcuts for business-day
steps and IMM rolls. `IsLiborTenor` / `IsSwapTenor` classify tenor strings: a
tenor containing `Y` (or `y`) is a swap tenor; anything else is a Libor
tenor.

## Schedules and day counts

A `Schedule_` is a `Vector_<Date_>` (`dal-cpp/dal/time/schedules.hpp`).
`DateGenerate` lays out an unadjusted strip at a fixed tenor, forward from
the start or backward from the maturity (`DateGeneration_`), and
`MakeSchedule` applies holiday adjustment on top. `MakeSchedulePeriods`
returns `SchedulePeriod_` records carrying unadjusted and accrual dates,
fixing and payment dates with independent lags and calendars, a stub flag,
and a `DayBasis::Context_` for coupon-aware day counts.

`DayBasis_` (`dal-cpp/dal/time/daybasis.hpp`) is the extensible day-count
enumeration — `ACT_365F`, `ACT_365L`, `ACT_360`, `ACT_ACT`, and `BOND`
(30/360). Calling a basis with start and end dates, plus an optional coupon
context, returns the year fraction used for accrual.

## Examples

No dedicated example program exercises the date machinery in isolation; the
snippet below is drawn from the public headers in `dal-cpp/dal/time/` and shows
the typical call sequence. Every symbol matches the current signatures in
`date.hpp`, `dateincrement.hpp`, `holidays.hpp`, `daybasis.hpp`,
`datetime.hpp`, and `schedules.hpp`.

```cpp
// Inline snippet drawn from the public headers in dal-cpp/dal/time/.
#include <dal/time/date.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/datetime.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/schedules.hpp>

using namespace Dal;

// Date_ wraps a validated serial day count; years span [1900, 2199] and a
// default-constructed Date_ fails IsValid().
const Date_ today(2026, 4, 30);
const int excelSerial = Date::ToExcel(today);
const Date_ naiveSpot = Date::AddMonths(today, 2);   // T+2 spot, naive month roll

// Holidays_ unions named centers; the empty string means no holidays. The
// free functions in namespace Holidays roll and adjust against a calendar.
const Holidays_ target("TARGET");
const Date_ spot    = Holidays::Adjust(target, naiveSpot, BizDayConvention_("ModifiedFollowing"));
const bool  isBizDay = Holidays::IsBusinessDay(target, spot);

// Date::Increment_ is the step interface; ParseIncrement covers tenors ('3M',
// '10Y', '2W'), business-day counts with a calendar suffix ('5BD;CN.IB'), and
// special-day names ('IMM', 'EOM'). operator+ applies FwdFrom.
const Handle_<Date::Increment_> tenor = Date::ParseIncrement("3M");
const Date_ maturity                   = spot + *tenor;

// DateTime_ pairs a Date_ with a sub-day fraction; the (hour, minute) ctor is
// used for fixing timestamps.
const DateTime_ fixingTime(spot, 11, 0);

// Schedule generation and year fractions. MakeSchedule lays out an adjusted
// strip; a DayBasis_ called as a functor returns the accrual fraction.
const Schedule_ dates  = MakeSchedule(today, maturity, PeriodLength_("3M"), target);
const DayBasis_ act365("ACT_365F");
const double yearFrac = act365(dates[0], dates[1], nullptr);
```

The build-tree test binary holds the executable spec for these types — for
example `./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter=DateTest.*`
covers construction, comparison, and month arithmetic, and `--gtest_filter=...`
also has dedicated suites for `DateIncrementTest`, `DateTimeTest`,
`DayBasisTest`, `ChinaCalendarTest`, and `HolidayDataTest`.

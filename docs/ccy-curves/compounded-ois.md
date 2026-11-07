# Explicit compounded OIS scripts

`NewCompoundedOisScriptInstrument` prices fixed contracts from explicit overnight
observations. Floating coupons are `N*(product(1+alpha_i*L_i)-1)`, discounted at
actual payment dates. `NewPeriodForwardOisScriptInstrument` retains its existing
whole-period forward representation. A mixed historical/future coupon needs
the explicit product. The [financial contract](../methodology/script_calibration_contract.md#explicit-compounded-ois-table-preparation)
defines table admission, identities and numerical errors.

Supply fixed-leg, coupon and observation tables, explicit ACT/360 or ACT/365F
bases, resolved calendar, currency/collateral, routes, direction, positive N
and fixed decimal K. `rate_start,rate_end` describe the complete overnight
rate interval; `start,end,accrual` describe its intersection with the coupon.
A Friday fixing through a Monday holiday uses one four-day factor.

Times are midnight/date-only. Strictly past fixings require the explicit
snapshot; future fixings always project. Set `Model` or `RequireHistorical`
explicitly for today. Empty snapshots are authoritative. Structural preparation
freezes inputs without sampling rates. Settled payments remain in source and
reports but have no price dependency. Live payments and complete rate intervals
must fit their bound curves. Short discount or passive base curves are valid
when they cover every actual query.

## C++ example

[`compounded_ois_script.cpp`](../../dal-public/examples/compounded_ois_script/compounded_ois_script.cpp)
uses `<dal-public/src/calibrationinstrument.hpp>` and links `DAL::public`.
Its target, CTest and installed executable names are `compounded_ois_script`.
From the repository root:

```bash
cmake --preset core-dev -DDAL_BUILD_PUBLIC=ON -DDAL_PUBLIC_BUILD_EXAMPLES=ON
cmake --build build/core-dev --target compounded_ois_script --parallel
ctest --test-dir build/core-dev -R '^compounded_ois_script$' --output-on-failure
```

It uses N=100 USD, PayFixed, K=0.02 and valuation 2024-03-01 with `Model`.
A negative Feb-29 historical fixing precedes the Mar-1/Mar-5 interval across
the weekend and Mar-4 holiday, then Mar-5/Mar-6. Both legs pay on Mar-8.
The explicit target serves discount and ON projection roles. Price is about
0.0137911322 USD and ParRate 0.0283954802.

Call `PrepareCompoundedOis(spec, valuation)`, then
`NewCompoundedOisScriptInstrument(tables)` and
`EvaluateCurveScriptInstrument(instrument, request)` with the same tables in
`request.compoundedOis_`. Defaults are `Price,ParRate`, `AutoFast`, derivative
`None` and FD step `1e-6`. Request `AAD` or `FiniteDifference` for partials.
Check success and availability before calling `CurveScriptEvaluationValue`
or `CurveScriptEvaluationPartials`. `CurveScriptEvaluationTable` projects
summary, measures, parameters, cashflows and observations.

Partial rows follow measure order; columns follow `freeParameters_`.
Price partials retain currency, actual N and direction; ParRate partials have
decimal-rate units per native coordinate. The example uses a log discount
factor. These are raw native derivatives, without automatic `1e-4` scaling
or quote DV01 interpretation. Its missing-fixing example retains an owning
report after request, preparation, source and exception destruction.

## Python calling recipe

```python
import dal

a, today, b, end, pay = [dal.Date_(2024, m, d) for m, d in
                        [(2, 29), (3, 1), (3, 5), (3, 6), (3, 8)]]
observations = [
    dal.CompoundedOisObservationRow_(start=start, end=stop,
        fixing_time=dal.DateTime_(start, 0), accrual=days / 360,
        coupon_id="coupon", rate_start=start, rate_end=stop)
    for start, stop, days in [(a, today, 1), (today, b, 4), (b, end, 1)]]
spec = dal.CompoundedOisScriptSpec_(
    instrument_id="mixed-sofr", direction="PayFixed", notional=100, coupon=0.02,
    currency="USD", collateral_currency="USD", collateral_type="OIS",
    discount_route="USD.OIS", projection_route="USD.SOFR", fixing_index="USD-SOFR",
    observation_basis="ACT_360", fixed_basis="ACT_365F",
    calendar=dal.HolidayCenterData_(center="explicit-Mar4", holidays=[dal.Date_(2024, 3, 4)]),
    fixed_leg=[dal.CompoundedOisFixedRow_(start=a, end=end, payment=pay, accrual=6 / 365)],
    coupons=[dal.CompoundedOisCouponRow_(coupon_id="coupon", start=a, end=end, payment=pay)],
    observations=observations)
valuation = dal.ScriptValuationSettings_(evaluation_date=today, today_fixing="Model",
    fixings=dal.MarketFixingSnapshot_New({"USD-SOFR": [(dal.DateTime_(a, 0), -0.012)]}))
tables = dal.CompoundedOis_Prepare(spec, valuation)
instrument = dal.CompoundedOisScriptInstrument_New(tables)
request = dal.CurveScriptEvaluationRequest_(
    geometry=dal.CurveInstrumentGeometry_(today=today, currency="USD",
        knot_dates=[today, today.AddDays(20)], parameterization="LOG_DISCOUNT",
        calibrate_discount_curve=False, target_overnight_route="USD.SOFR"),
    market=dal.CurveInstrumentMarket_(routes=[
        dal.CurveScriptRoute_(id="USD.OIS", use_target_curve=True),
        dal.CurveScriptRoute_(id="USD.SOFR", projection=True, overnight=True)]),
    valuation=valuation, parameters=[-0.002], compounded_ois=tables, derivative_method="AAD")
result = dal.CurveScriptInstrument_Evaluate(instrument, request)
price = dal.CurveScriptEvaluation_Get_Value(result, "Price")
par = dal.CurveScriptEvaluation_Get_Value(result, "ParRate")
partials = dal.CurveScriptEvaluation_Get_NativePartials(result)
assert abs(price - 0.013791132165949717) <= 1e-10
assert abs(par - 0.028395480180666424) <= 1e-10
```

Strict conversion finishes before core work releases the GIL. Results and
errors own their data; container getters return independent copies.
See the [Python interface](../python/README.md#explicit-compounded-ois).

## Excel calling recipe

Use the same dates, scalar fields and three observation rows above with the
named headers in the [Excel contract](../excel/README.md#explicit-compounded-ois).
Create `COMPOUNDEDOISFIXEDLEG.NEW`, `COMPOUNDEDOISCOUPONS.NEW`,
`COMPOUNDEDOISOBSERVATIONS.NEW` and `COMPOUNDEDOISCALENDAR.NEW` handles,
then `COMPOUNDEDOISSCRIPTSPEC.NEW`. The calendar's `holidays` field references
a `CALIBRATIONDATES.NEW` table containing 2024-03-04.

Create the explicit valuation snapshot with the Feb-29 fixing and `Model`,
then `COMPOUNDEDOIS.PREPARE(spec,valuation)` and
`COMPOUNDEDOISSCRIPTINSTRUMENT.NEW(tables)`. The six-column
`CURVESCRIPTOVERNIGHTROUTES.NEW` table requires blank `tenor` values for ON.
USD.OIS declares OIS collateral, `projection=FALSE,overnight=FALSE,
use_target_curve=TRUE`; USD.SOFR declares OIS collateral,
`projection=TRUE,overnight=TRUE,use_target_curve=FALSE`.

Pass geometry, market, valuation, parameters and tables to
`CURVESCRIPTEVALUATIONREQUEST.NEW` with `derivative_method=AAD`, then call
`CURVESCRIPTINSTRUMENT.EVALUATE(instrument,request)`. Check
`CALIBRATIONOUTCOME.GET.STATUS` before `CURVESCRIPTEVALUATION.GET.VALUE`,
`.GET.NATIVEPARTIALS` or `.GET.TABLE`. Valid numerical failures retain
`Failed` plus an owning report; malformed contexts/formats raise worksheet
errors. Complete `cell[][]+` selections are validated, including unknown or
duplicate content after blank rows, with original row/column positions.
Unavailable values stay blank and numeric getters reject them.

## Resource measurement

Configure `core-dev` with `-DDAL_CPP_BUILD_BENCHMARKS=ON`, build
`curve_calibration_perf`, then run:

```bash
DAL_NUM_THREADS=4 build/core-dev/dal-cpp/benchmarks/curve_calibration_perf/curve_calibration_perf --compounded-ois
DAL_NUM_THREADS=4 build/core-dev/dal-cpp/benchmarks/curve_calibration_perf/curve_calibration_perf --ois-neighbor
```

The first mode uses D=30/365/1825 intervals, one live coupon, N=1, K=0.025,
eight free log-DF coordinates on fixed 400-day knot spacing, and `floor(D/2)`
historical observations. Independent scalar Price/ParRate and two-step FD/AAD
checks precede timing. A separate parser-alias probe preserves canonical ON
semantics: D parses at source construction, zero during prepared evaluation.
That additional source construction is excluded from timed preparation.

Output separates table/source/two-plan preparation, prepared passive values,
AAD with two reverse seeds, same-size scalar product and future curve queries.
Fresh preparation/scratch timings and initial tape capacity identify cold
state; warm AAD still allocates request scratch. Curve interpolation uses
fixed node count; O(D) is not claimed when nodes also grow. No D-by-D matrix
or per-fixing full product recomputation is used.

Resource rows measure instruction payload, scratch objects plus dynamic array
capacities, forward-peak tape nodes/live/reserved bytes and post-scope cleanup.
`scratch_tape_peak_payload` accounts for scratch plus live tape; it excludes
source/AST/descriptor strings, curves, allocator overhead and returned axes.
Linux RSS/high-water RSS include other process data; zero means unavailable
on other platforms. Reserved capacity after cleanup remains real resource use.

For the adjacent LOG_LINEAR analytic/bumped calibration cases, retain two
rounds of ten alternating process pairs, best-of-ten minima, the existing 4%
threshold and all raw samples. This executable is outside the nine-target
scheduled regression gate; its comparisons are scoped acceptance evidence.
Compare the new O(D) product with the same-size scalar reference, never the
old O(1) period-forward product as a regression. Performance statements are
limited to the measured build, machine and workload.

P1 quote-risk snapshots/per-PV, general script/MC and legacy tenor staging
continue to reject ON. Automatic RFR schedules, lookback, lockout, observation
shift, intraday publication rules and scenario revaluation remain outside scope.

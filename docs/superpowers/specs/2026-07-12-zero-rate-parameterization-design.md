# ZERO_RATE Curve Parameterization Design

Date: 2026-07-12

Status: Approved for implementation planning

## Objective

Implement `CurveParameterization_::ZERO_RATE` as a complete, persistent yield-curve
representation. The representation must support direct construction, single and joint
calibration, AAD analytical Jacobians, base-curve layering, archive round-trips, and the
existing public C++, Python, and Excel surfaces.

ZERO_RATE uses continuously compounded zero rates as its parameter and bump coordinates,
but maps those node values to log discount factors before applying DAL's shared log-DF
interpolation framework.

## Representation and Mathematical Contract

Let `a` be the curve anchor and let each declared knot `d_i` be strictly after `a`.
The curve stores one finite, continuously compounded zero rate `z_i` per future knot.
Using the curve's configured day-count basis,

```text
t_i   = YearFrac(a, d_i)
ell_i = -z_i * t_i
```

The interpolation geometry is constructed over

```text
times:  [0, t_1, ..., t_N]
values: [0, ell_1, ..., ell_N]
```

For arbitrary dates `from` and `to`, the zero-rate component is

```text
P_zero(from, to) = exp(ell(to) - ell(from))
```

and a layered curve evaluates as

```text
P_total(from, to) = P_zero(from, to) * P_base(from, to).
```

When a base is present, stored zero rates therefore represent continuously compounded
spreads over the base, not total base-composed zero rates.

The anchor has an exact component log discount factor of zero. It is not a zero-rate node
or a solver parameter, so the implementation never computes or exposes `z(0)` and never
divides by a zero year fraction. Zero and negative finite rates are valid.

## Interpolation and Extrapolation

ZERO_RATE must reuse `LogDfInterpolation_` without changing its algorithms. The mapped log
discount factors are the interpolation ordinates for all existing schemes:

- `LOG_LINEAR`
- `LOG_CUBIC_NATURAL`
- `MIXED`

Consequently, ZERO_RATE and an equivalent `DiscountLogDF_` curve constructed from the
same mapped ordinates must agree at nodes, between nodes, before the anchor, and beyond
the last node.

Existing scheme semantics remain authoritative. In particular, right-tail extrapolation
uses the secant through the last two mapped log-DF nodes, rather than a flat last zero
rate. No new zero-rate interpolation or extrapolation scheme is introduced.

After the implicit anchor is included, the minimum future-knot counts are:

- one for `LOG_LINEAR`
- two for `LOG_CUBIC_NATURAL`
- three for `MIXED`

## Persistent Curve Type

Add `Tape::DiscountZeroRate_<T_, B_>` and its passive alias `DiscountZeroRate_`. The class
inherits the existing discount-curve, base-layering, and fittable-curve abstractions.

It stores:

- name and currency
- a separate anchor date
- future knot dates only
- one typed zero-rate ordinate per future knot
- day-count basis
- `LogDfScheme_`
- optional base curve
- passive interpolation geometry built from the anchor-relative year fractions

The public accessors distinguish the anchor from the future knots:

- `AnchorDate()`
- `NodeDates()` returning future knots only
- `NodeZeroRates()` returning one value per future knot
- `DayCount()`
- `Scheme()`

`NX()` returns the number of future zero-rate nodes. `ApplyDX` bumps the stored zero rates
in future-knot order, so persistent risk coordinates match calibration and Jacobian
coordinates. `Clone` preserves representation, dates, zero rates, day count, scheme, and
normal base substitutions.

For active curves, mapped ordinates must be formed from active zero-rate values after AAD
recording begins. Passive year fractions and interpolation weights multiply those active
values without extracting them through `double`.

## Definition and Parameter Layout

`CurveDefinition_` must retain the supplied anchor. For ZERO_RATE,
`MakeCurveDefinition` requires strictly increasing, future-only declared knots and prepends
the anchor exactly once to its internal storage dates.

For `N` declared future knots, `BuildCurveParameterLayout` returns:

```text
storageNodeCount        = N + 1
parameterCount          = N
paramsPerDeclaredKnot   = 1
pinnedAnchor            = true
```

Parameter, smoothing, AAD, diagnostic, and archive zero-rate order is the declared future
knot order. The anchor contributes no parameter or Jacobian column.

The existing `pinnedAnchor_` flag cannot be used as a proxy for whether the caller's input
knot vector contains the anchor. LOG_DISCOUNT single calibration currently supplies the
anchor explicitly, whereas ZERO_RATE supplies future knots only. Free-node and smoothing
dates must instead be derived from `CurveDefinition_` and `CurveParameterLayout_`.

## Single Calibration

Single calibration must accept future-only ZERO_RATE knot dates and continue to require an
explicit anchor as knot zero for LOG_DISCOUNT. Existing PWC, PWL, and LOG_DISCOUNT input
validation and numerical behavior must remain unchanged.

Initial guesses are parameterization-aware:

- ZERO_RATE `initialGuess_` is a continuously compounded decimal zero rate and is copied
  to all free nodes.
- ZERO_RATE `initialGuessPerNode_` contains one zero rate per future knot.
- A supplied `initialGuessPerNode_` must be finite and have exactly the layout's parameter
  count.
- LOG_DISCOUNT retains its current default flat-rate-to-logDF transformation.
- Existing forward-parameterization default scalar guesses remain unchanged.

Smoothing weights use the definition's free future dates, never the raw input shape.
EXACT, APPROXIMATE, and staged single-curve calibration all use the same typed factory and
must return a persistent `DiscountZeroRate_`.

## Joint Calibration and Base Layering

Joint declarations already use future-only knots. ZERO_RATE joins the existing typed
factory, stacked parameter layout, smoothing, and two-pass active-base construction.

Required mixed cases include:

- ZERO_RATE discount plus a non-ZERO_RATE forward curve
- a non-ZERO_RATE discount plus a base-layered ZERO_RATE forward curve
- homogeneous ZERO_RATE discount and forward curves

Stacked parameter blocks remain declaration ordered and future-knot ordered. AAD tests must
show local ZERO_RATE sensitivities and cross-curve sensitivities through an active base.

## Analytical Jacobian

At a stored node,

```text
d ell_i / d z_j = -t_i * delta_ij.
```

At off-node dates, the existing interpolation weights propagate these derivatives. The
single and joint residual Jacobians must use the same zero-rate column order and units as
central zero-rate bumps and `ApplyDX`.

Remove only the explicit ZERO_RATE analytic-eligibility rejection. All unrelated instrument,
routing, solve-mode, and day-count eligibility/fallback gates remain unchanged.

Analytical forward Jacobians, effective inverse-Jacobian diagnostics, central-difference
oracles, and bumped calibration results must agree within the tolerances already used by
the existing calibration test suites.

## Archive Contract

Add a new, additive `DiscountZeroRate_v1` storable schema with:

```text
name
ccy
anchorDate
nodeDates
zeroRates
dayCount
scheme
base
```

The canonical reader reconstructs `DiscountZeroRate_`, not `DiscountLogDF_`. A round-trip
must preserve the dynamic type, zero-rate coordinates, scheme, day count, base, and node,
interior, and extrapolated values. Only the passive curve with a passive discount-curve
base is serializable, matching the existing `DiscountLogDF_` policy.

The schema fixes continuous compounding; it contains no compounding selector. Supporting
other compounding conventions later requires an explicit new API/schema decision rather
than reinterpreting v1.

## Public APIs

Core factory:

```cpp
DiscountCurve_* NewDiscountZeroRate(
    const String_& name,
    const String_& ccy,
    const Date_& anchorDate,
    const Vector_<Date_>& nodeDates,
    const Vector_<>& zeroRates,
    const DayBasis_& dayCount,
    LogDfScheme_ scheme,
    const Handle_<DiscountCurve_>& base = {});
```

Public C++ factory:

```cpp
Handle_<DiscountCurve_> DiscountZeroRateNew(
    const String_& name,
    const String_& ccy,
    const Date_& anchorDate,
    const Vector_<Date_>& nodeDates,
    const Vector_<>& zeroRates,
    const DayBasis_& dayCount = DayBasis_("ACT_365F"),
    LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR,
    const Handle_<DiscountCurve_>& base = {});
```

Python factory:

```python
DiscountZeroRate_New(
    name,
    ccy,
    anchor_date,
    node_dates,
    zero_rates,
    day_count=DayBasis_("ACT_365F"),
    log_df_scheme=LogDfScheme.LOG_LINEAR,
    base=None,
)
```

Excel function:

```text
DISCOUNTZERORATE.NEW(
    name, ccy, anchorDate, nodeDates, zeroRates,
    [dayCount], [logDfScheme], [base])
```

Existing Python and Excel parameterization settings already expose the ZERO_RATE enum and
become functional. The Python high-level single-calibration helper and Excel
`CALIBRATE.SINGLECURVE` also accept an optional base curve for ZERO_RATE spread calibration.
No new Python or Excel joint-calibration surface is added solely for this feature.

Update the `LogDfScheme_` description to cover both LOG_DISCOUNT and ZERO_RATE, then run the
normal core and Excel Machinist generation passes. Existing enum names and ordinals remain
unchanged.

## Validation and Errors

Construction rejects:

- empty future-node arrays
- date/rate length mismatch
- duplicate or non-monotonic dates
- any node date at or before the anchor
- non-finite zero rates
- non-finite, non-positive, or non-monotonic anchor-relative year fractions
- insufficient future nodes for the selected interpolation scheme

Day-count evaluation uses the configured `DayBasis_` with the same null context convention
as the existing log-DF curve. Context-dependent bases such as ACT/365L therefore fail with
their existing explicit context requirement rather than being silently approximated.

## Test Strategy

Tests are added before production implementation and cover:

1. Definition and layout: future-only declarations, one implicit anchor, exact
   `N+1/N/1/true` layout, and rejection of a declared anchor node.
2. Direct construction: anchor identity, same-date identity, exact node formula, arbitrary
   `from/to`, inverse identity, zero and negative rates, ACT/365F versus ACT/360, and scheme
   minimum-node validation.
3. Shared interpolation: for every scheme, agreement with an independently constructed
   `DiscountLogDF_` oracle at nodes, interior dates, before anchor, and after the final node.
4. Fittable behavior: `NX`, zero-rate-coordinate `ApplyDX`, inverse bumps, passive base, and
   active-base propagation.
5. Persistence: clone and JSON/archive round-trips without a base and with a serializable
   base, preserving the dynamic type and zero-rate coordinates.
6. Single calibration: EXACT, APPROXIMATE, and staged ZERO_RATE cases, scalar and per-node
   seeds, analytical versus bumped solutions, residuals, and central-difference Jacobians.
7. Joint calibration: homogeneous and heterogeneous ZERO_RATE blocks, including a
   base-layered ZERO_RATE forward curve and cross-block AAD sensitivities.
8. Public surfaces: public C++ construction/calibration, Python construction/calibration,
   Excel generated construction and calibration contracts, and Machinist drift checks.
9. Regression: existing curve, calibration, archive, public, Python, and generated-file
   suites remain green on the supported AAD configurations.

## Documentation

Update current-state documentation to:

- remove the statement that ZERO_RATE is unsupported
- define continuous compounding and day-count mapping
- describe the absent anchor parameter and t=0 behavior
- describe all shared interpolation and extrapolation semantics
- describe base-spread, initial-guess, AAD, and Jacobian coordinates
- list the core, public C++, Python, and Excel factories

Record the new numerical capability in `CHANGELOG.md`. Do not add historical implementation
narrative to current-state methodology or public API documents.

## Compatibility and Non-Goals

This is an additive representation and archive type. Existing enum identities, default
parameterization, curve schemas, LOG_DISCOUNT/PWC/PWL layouts, interpolation algorithms,
calibration results, and fallback behavior must remain unchanged.

The following are explicitly out of scope:

- non-continuous compounding conventions
- direct interpolation of zero rates
- a free zero-rate parameter at the anchor
- new interpolation or extrapolation algorithms
- broadening unrelated AAD eligibility gates
- adding a new public joint-calibration API

# XCCY Resettable/MTM Notional and In-Progress Trades — Design

## Source

- User request and design decisions confirmed on 2026-07-13.
- Related methodology: `docs/methodology/xccy_calibration.md` and `docs/methodology/yield_curve_jacobian.md`.

## Problem Statement

The current cross-currency swap implementation supports only fixed domestic and foreign
notionals and rejects valuation after the swap start. It also reads no historical FX
fixings and calibrates only a basis curve over frozen domestic and foreign curve blocks.
The extension must support period-by-period resettable and mark-to-market notionals,
deterministic valuation of in-progress trades, staged basis calibration, and a single
joint solve of domestic, foreign, and basis curves with an analytic residual Jacobian.

## Goals

- Define unambiguous notional, FX fixing, reset timing, historical cashflow, and notional
  exchange rules.
- Use one pricing formula for passive `double` valuation and active AAD valuation.
- Preserve the existing fixed-notional public builders and basis-only calibration entry
  point.
- Add public C++, Python, and Excel surfaces for reset configuration, fixing snapshots,
  and three-block joint calibration.
- Make missing or contradictory fixing data fail deterministically before solving.
- Expose enough Jacobian layout metadata that callers do not infer parameter or residual
  block positions.

## Non-Goals

- Third-currency collateral, collateral switching, or cheapest-to-deliver collateral.
- Resetting the foreign leg.
- Independent reset frequency, intraperiod resets, or reset dates not aligned with the
  domestic coupon accrual starts.
- Intraday settlement status for cashflows or separate clean/accrued valuation.
- A general-purpose multi-currency cashflow graph.

## Product Semantics

### Notional Modes

Replace the existing `resettableNotional_` and `markToMarketNotional_` booleans with one
generated enum, `XccyNotionalMode_`:

- `FIXED`: both notionals remain fixed for the full trade.
- `RESETTABLE`: the foreign notional remains fixed; the domestic coupon notional is reset
  for each domestic accrual period. No intermediate notional-difference cashflow is paid.
- `MARK_TO_MARKET`: uses the same reset domestic coupon notionals as `RESETTABLE` and also
  exchanges each change in domestic notional on the reset effective date.

The first domestic coupon period uses the builder-supplied domestic notional. Starting
with the second domestic coupon period,

```text
domesticNotional[i] = foreignNotional * FX(fixingTime[i])
```

where FX is quoted in domestic currency units per one foreign currency unit. For
`MARK_TO_MARKET`, the domestic notional exchange at the reset effective date is

```text
domesticNotional[i] - domesticNotional[i - 1]
```

with the same receive/pay orientation as the domestic final-notional exchange. The
pricing kernel applies the overall leg signs consistently when forming the par spread.

### Reset Timing

- The domestic leg is the resetting leg; the foreign leg notional is fixed.
- Each reset effective date is the corresponding domestic coupon accrual start.
- The FX fixing date is obtained by moving backward by `fixingLag_` business days on
  `fixingHolidays_`, then applying `fixingConvention_`.
- The fixing timestamp combines that date with configured `fixingHour_` and
  `fixingMinute_`.
- The first period uses the supplied initial domestic notional rather than recomputing it
  from a fixing.
- Domestic and foreign coupon frequencies may differ. The domestic reset schedule must
  align exactly with domestic coupon accrual starts.

### Initial, Intermediate, and Final Notional Exchanges

- `initialNotionalExchange_` controls the builder-supplied initial domestic and foreign
  exchanges at the trade start for every notional mode.
- Only `MARK_TO_MARKET` generates intermediate domestic notional-difference exchanges.
- `finalNotionalExchange_` exchanges the last outstanding domestic notional and the fixed
  foreign notional at maturity.
- Historical notional exchanges follow the same payment-date filtering rule as coupons.

## Fixing Data Contract

### Immutable Snapshot

`FxFixingSnapshot_` is an immutable mapping from normalized FX index name and `DateTime_`
to a positive finite value. Calibration and valuation consume a snapshot, never live
global fixing state.

Public builders accept an optional snapshot. If none is supplied, the valuation entry
point copies all required FX fixings from the existing process-wide fixing store once,
before precomputation or solver evaluation. Subsequent global-store changes cannot alter
that valuation or calibration.

### Naming and Orientation

- Canonical keys retain the existing `Index::Fx_` naming convention:
  `FX[foreign/domestic]` for a domestic-per-foreign quote.
- A reverse fixing may be used by taking its reciprocal.
- If both orientations exist at the same timestamp, their product must equal one within
  a fixed numerical tolerance; otherwise snapshot construction fails.
- Zero, negative, NaN, and infinite fixing values are rejected.

### Historical and Future Resolution

- `fixingTime < valuationTime`: a historical fixing is mandatory; missing data is an
  error and cannot be replaced with a curve-implied value.
- `fixingTime == valuationTime`: use a supplied fixing if present; otherwise treat the
  reset as unfixed and use the current FX forward.
- `fixingTime > valuationTime`: use the current basis-adjusted FX forward.

Historical resolved values are passive constants. Today-unfixed and future values retain
the pricing scalar type so their derivatives flow through domestic, foreign, and basis
curve parameters.

## Historical Cashflow Rules

The first version uses date-level settlement because the existing XCCY market exposes a
valuation date but no intraday payment-settlement state.

- `paymentDate < valuationDate`: the cashflow is settled and excluded from PV, par
  spread, residuals, and Jacobians.
- `paymentDate == valuationDate`: the cashflow is unsettled, included with discount
  factor one.
- A coupon fixed in the past but paid on or after the valuation date uses its historical
  fixing and remains in PV.
- Historical initial and MTM notional-adjustment cashflows are excluded.
- A future final exchange uses the last outstanding domestic notional and the fixed
  foreign notional.
- Valuation is dirty/full PV; no separate accrued-interest output is introduced.

A matured trade may be represented and has zero remaining PV. It cannot be used as a
calibration instrument when it has no remaining spread annuity.

## Collateral and Currency Scope

- The pricing currency and collateral currency are both the currency pair's domestic
  currency.
- `CrossCurrencyMarket_` and both calibration specifications record the collateral
  currency explicitly.
- The first implementation rejects `collateralCurrency != pair.domestic_`.
- Foreign cashflows are converted to domestic currency with the basis-adjusted FX forward
  generated by the same market view used to discount and forecast them.

This explicit restriction prevents a missing third-currency curve from being silently
substituted by an OIS fallback.

## Internal Architecture

### `XccyCashflowPlan_`

An immutable plan created by `CrossCurrencySwap_::Precompute()` contains both legs'
coupon periods, accrual factors, fixing and payment dates, domestic reset dates, all
potential notional exchanges, notional mode, and spread-leg selection. It describes
events but performs no curve or fixing lookup.

### `FxResetResolver_`

The resolver takes a valuation time, fixing snapshot, and current typed XCCY market view.
It produces period domestic notionals and MTM differences using historical constants or
active FX forwards according to the fixing contract.

### `XccyPricingKernel<T_>`

One templated kernel computes coupon PVs, initial/intermediate/final notional PVs,
foreign-to-domestic conversion, annuities, par spreads, and calibration residuals.
`T_ = double` supports passive pricing and manual cashflow tests;
`T_ = AAD::Number_` supports analytic Jacobians. Historical cashflows are filtered before
summation.

### Joint Residual Function

The joint unknown vector has the stable partition

```text
[domestic curve parameters | foreign curve parameters | basis parameters]
```

Each residual evaluation builds all three typed curve views once and evaluates a stacked
residual vector containing domestic instruments, foreign instruments, and XCCY
instruments. It never calls a nested calibration routine. Future resets therefore refer
to the active curves from the current solver iteration, eliminating procedural curve
cycles while retaining the mathematical coupling in one nonlinear system.

The existing basis-only calibration uses the same pricing kernel with frozen domestic and
foreign blocks and only the basis parameter partition active.

## Public API

### Instrument Configuration

The new public shape is a configuration object rather than more positional arguments:

```cpp
struct FxResetConvention_ {
    XccyNotionalMode_ mode_ = XccyNotionalMode_::Value_::FIXED;
    int fixingLag_ = 0;
    Holidays_ fixingHolidays_;
    BizDayConvention_ fixingConvention_ = BizDayConvention_("Preceding");
    int fixingHour_ = 0;
    int fixingMinute_ = 0;
};

struct CrossCurrencySwapConfig_ {
    CurrencyPair_ pair_;
    double domesticNotional_ = 100.0;
    double foreignNotional_ = 100.0;
    CrossCurrencyConvention_ convention_;
    FxResetConvention_ reset_;
};
```

The new overload is:

```cpp
Handle_<CrossCurrencySwap_> CrossCurrencySwapNew(const Date_& tradeDate,
                                                  const Date_& start,
                                                  const Date_& maturity,
                                                  double marketRate,
                                                  const CrossCurrencySwapConfig_& config);
```

The current long overload remains source compatible and delegates to a `FIXED` config.
`CrossCurrencySwapConfigBuilder_` supplies a binding-friendly construction path.

### Basis-Only Calibration

`CrossCurrencyCalibrationSpecBuilder_` adds `valuationTime_`,
`collateralCurrency_`, and optional `fxFixings_`. The existing `today_` field remains:
when `valuationTime_` is not set, it maps to midnight on `today_`.

### Three-Block Joint Calibration

`JointXccyCalibrationSpecBuilder_` contains:

- valuation time, currency pair, domestic collateral currency, and FX spot;
- domestic and foreign `JointCurveDeclaration_` collections;
- one basis curve declaration;
- domestic, foreign, and XCCY instrument collections;
- optional FX fixing snapshot;
- one shared `CurveSolverOptions_`.

The implementation factors the existing joint curve declaration and solver machinery into
reusable internal helpers rather than duplicating solver control fields.

The joint result carries solved domestic and foreign `CurveBlock_` objects, basis and FX
forward curves, diagnostics grouped by residual class, the stacked residual Jacobian, and
explicit parameter/residual block ranges.

### Python and Excel

- Python mirrors the enum, reset convention, swap config/builder, fixing snapshot, joint
  builder, result, diagnostics, and range metadata. Snapshot construction accepts a
  nested `{index_name: {datetime: value}}` mapping.
- Excel retains the existing `CROSSCURRENCYSWAP.NEW`. New worksheet functions create an
  FX reset convention/config handle and build a configured XCCY swap without changing the
  old registration. Another function creates an immutable FX fixing snapshot from index,
  datetime, and value ranges.
- `CALIBRATE.JOINTXCCY` accepts curve and instrument handles plus a settings dictionary.
  Result accessors expose curves, diagnostics, Jacobian, and block ranges.
- Machinist regenerates both core and Excel enum artifacts.

## Error Contract

Errors name the instrument where applicable and include the offending currency pair,
collateral currency, fixing timestamp, or curve slot. Validation rejects:

- unsupported collateral currency;
- missing historical fixing;
- inconsistent direct/reverse fixings;
- nonpositive or nonfinite fixing/notional/spot values;
- invalid fixing hour/minute or negative fixing lag;
- reset dates not aligned with domestic coupon accrual starts;
- currency-pair, curve-block, basis-orientation, or valuation-date mismatch;
- a calibration instrument with no remaining spread annuity;
- an analytic Jacobian request for an ineligible parameterization, without silent fallback.

## Test Strategy and Acceptance Criteria

### Cashflow and Fixing Unit Tests

- [ ] `FIXED`, `RESETTABLE`, and `MARK_TO_MARKET` plans contain the expected coupon and
  notional events.
- [ ] Fixing lag, holidays, business-day adjustment, and a stub schedule produce exact
  expected dates.
- [ ] Direct, reverse, duplicate-consistent, duplicate-inconsistent, missing, nonpositive,
  and nonfinite fixings follow the contract.
- [ ] Past, equal-to-valuation, and future fixing timestamps select the intended source.
- [ ] Changing the global fixing store after snapshot creation cannot change valuation.

### Pricing and In-Progress Trade Tests

- [ ] Existing fixed-notional pricing results remain unchanged.
- [ ] Domestic notionals reset period by period to fixed foreign notional times FX.
- [ ] A two-period MTM swap matches a hand calculation of every coupon and notional
  exchange cashflow.
- [ ] An in-progress swap excludes settled payments, retains past-fixed/future-paid
  coupons, and includes valuation-date payments at discount factor one.
- [ ] A matured trade has zero PV and is rejected as a calibration instrument.

### Calibration and Jacobian Tests

- [ ] Existing basis-only calibration continues to converge and reprice.
- [ ] A synthetic joint system recovers domestic, foreign, and basis curve parameters in
  one solve and reprices all three instrument groups within tolerance.
- [ ] Exact and approximate solve modes are both covered.
- [ ] The analytic stacked Jacobian matches a central-difference Jacobian element by
  element across domestic, foreign, and basis parameter blocks.
- [ ] Historical resets have no fixing sensitivity; future resets have the expected curve
  sensitivities.
- [ ] Quote-to-parameter effective inverse results agree with explicit quote bump and
  recalibration.
- [ ] AAD tape state is cleared before and after each AAD test.

### Binding Tests

- [ ] Existing C++ and Python fixed-notional calls remain valid.
- [ ] C++/Python builders round-trip every new field.
- [ ] Python constructs a fixing snapshot, MTM swap, joint specification, and reads the
  Jacobian layout.
- [ ] Excel generated sources compile and smoke tests cover configured swap construction,
  fixing snapshot input, and joint calibration/result accessors.

## Delivery Sequence

1. Add the enum, reset/config types, immutable fixing snapshot, and validation.
2. Add cashflow planning, resolver, and passive/AAD pricing kernel with unit tests.
3. Integrate in-progress valuation and basis-only calibration without changing legacy
   fixed-notional results.
4. Add the single-system domestic/foreign/basis joint solve and Jacobian diagnostics.
5. Add public C++ builders, Python bindings, Excel functions/generated artifacts, examples,
   methodology documentation, public API documentation, and changelog entry.

Each stage must pass its targeted tests plus existing XCCY and joint-calibration regression
tests before the next stage begins.

## Risks and Mitigations

- **Fixing data contract:** immutable normalized snapshots isolate valuation from mutable
  process-wide state.
- **Collateral convention:** an explicit domestic-currency-only check prevents accidental
  third-currency curve fallback.
- **Curve cycles:** one stacked residual function and one unknown vector replace nested
  calibration calls.
- **Passive/AAD drift:** one templated pricing kernel owns all formulas.
- **API growth:** config/builders contain optional behavior while legacy overloads remain.
- **Scope growth:** nonaligned resets and third-currency collateral are explicit non-goals.

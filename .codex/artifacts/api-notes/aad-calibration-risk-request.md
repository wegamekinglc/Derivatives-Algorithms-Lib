# Calibration-coordinate request API

Status: active, locally implemented public API for the first request increment of F01.
See the [contract](../specs/aad-calibration-risk-request.md) for budget scope,
ownership, native methods and remaining automatic valuation work.

## Audiences and current surface

C++ callers and the Python/Excel bindings currently create
`CalibrationPullback_`, typed parameter/direct adjoints and `CalibrationQuoteRisk_`.
Those values remain the canonical native source and complete three-contribution
representation. The new request layer adds selected coordinate ordering,
report-only scaling and a preflight numeric-result budget.

## Common surface

```cpp
struct CalibrationRiskRequest_ {
    std::optional<Vector_<String_>> inputs_;
    std::optional<Vector_<>> reportFactors_;
    std::optional<size_t> numericPayloadBudgetBytes_;
};

size_t CalibrationRiskPayloadBytes(size_t quoteRows, size_t quoteCols);
CalibrationRiskPlan_ PlanCalibrationRiskRequest(
    const CalibrationPullback_& calibration,
    const CalibrationRiskRequest_& request = {});
CalibrationRiskResult_ PullbackCalibrationWithRisk(
    const CalibrationRiskPlan_& plan,
    const CalibrationParameterAdjoints_& parameters,
    const std::optional<CalibrationDirectQuoteAdjoints_>& direct = {});
```

The plan exposes `Calibration()`, `CompleteInputAxis()`, `InputAxis()`,
`SelectedOrdinals()` and `NumericPayloadBytes()`. Axes own source-scoped
`quote:<ordinal>` IDs and native coordinates. Report scales are applied to the
selected axis only. Dupire supplies spread values/strike/maturity; curves supply
block identity and no fabricated quote value.

The result exposes `Plan()` and `QuoteRisk()` by const reference. Its
`Jacobian()`, `CalibrationJacobian()`, `DirectJacobian()` and `ReportedJacobian()`
return detached 1-by-selected matrices. Only the three full native matrices are
retained. There is no value/PV getter because a generic external gradient does
not supply an authoritative product value or PV currency.

```cpp
CalibrationRiskRequest_ request;
request.inputs_ = Vector_<String_>{"quote:3", "quote:0"};
request.reportFactors_ = Vector_<>{0.01, 0.01};
const auto plan = PlanCalibrationRiskRequest(boundary, request);
const auto result = PullbackCalibrationWithRisk(plan, parameters, direct);
const auto raw = result.Jacobian();
const auto perVolPoint = result.ReportedJacobian();
```

Required owning source/plan/seeds precede optional request/direct parameters.
Configuration fields mirror scalar D04 names. Keep this in
`dal-public/src/calibrationriskrequest.hpp`; core script planning remains unaware
of public calibration types.

## Error and compatibility decisions

`InvalidCalibrationRiskRequest` identifies malformed IDs, factors, extents or
arithmetic. `CalibrationRiskBudgetExceeded` identifies the retained full numeric
payload limit. Existing `CalibrationSnapshotMismatch`/native shape/finite
diagnostics are preserved for wrong owning seeds. Dupire parameter seeds retain
full snapshot identity; direct seeds retain the accepted quote-only identity,
including quote values, while a different fixed base IVS is allowed. Curves keep
full identity for both kinds of seeds. IDs resolve only inside the
plan's complete owned source. They do not replace native identity checks.

Old signatures and default paths do not call or allocate the planner. Getters
never apply another VJP or multiply a previously reported copy. Reject source
mismatches instead of borrowing labels or accepting fingerprint equality alone.

## Rejected alternatives and later integration

- Send quote IDs to core `RiskRequest_`: violates dependency direction and its
  model/constant-only axis contract.
- Keep cached selected/report matrices: duplicates the native three matrices and
  makes budget accounting and detached-getter ownership harder to inspect.
- Estimate only selected quote cells: undercounts retained full contributions.
- Infer quote values/currencies from curve labels or parse captured JSON: loses
  typed source identity or invents missing native metadata.
- Wrap each native domain with a separate request implementation: duplicates
  selection, budget and report logic already expressible over the common source.

Automatic Dupire execution will use a separate immutable valuation plan owning
the common request plan and sealed product/model/settings, with mandatory surface
inputs explicitly exposed. Direct bindings use constant ordinals and quote IDs,
not display names. Its result also owns the scalar valuation; it must include
that value/Jacobian in the total numeric payload. Python/Excel factories/getters
will reuse these native plans and strict existing input guards. This increment
does not yet expose those future execution functions.

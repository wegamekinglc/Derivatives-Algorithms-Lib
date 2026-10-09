# Monte Carlo quote curvature API decisions

Status: active; controls the common-path financial adapter.

## Audience and existing surface

C++ callers currently create `DupireScriptRiskPlan_` and execute
`ValueByMonteCarloWithDupireRisk`. Native `EvaluateDupireQuoteCurvature` accepts
a scalar native objective and cannot wrap that parallel financial execution.
Keep the financial orchestration outside the native callback.

## Proposed surface

- Add `RecalibrateDupireScriptRisk(plan, quoteSpreads)` alongside the existing
  first-order plan. It returns an independent rebuilt plan, freezes fixing
  dependencies, and rejects external direct seeds and exercise contracts.
- In `dal-public/src/dupirecurvature.hpp`, add
  `DupireScriptCurvatureRequest_ { risk_, bumps_ }`,
  `PlanDupireScriptCurvature(product, model, calibration, component, request)`
  and `ValueByMonteCarloWithDupireCurvature(plan)`.
- The immutable plan exposes base financial plan, raw point, directions,
  steps, and numeric payload. The owning result exposes `Base()`, `Point()`,
  `InputAxis()`, `Directions()`, `Steps()`, `HessianProducts()`, and
  `Execution()`. Curvature is always in full raw quote coordinates; the base
  financial result retains requested first-order projection/reporting.
- Execution identifies `BumpOverRecalibratedNativeDupireMonteCarloAAD`, total
  quote-gradient evaluations, paths per evaluation and numeric payload bytes.

```cpp
DupireScriptCurvatureRequest_ request;
request.risk_.numPaths_ = 257;
request.risk_.valuation_.evaluationDate_ = Date_(2026, 9, 12);
request.bumps_.directions_ = Matrix_<>(1, 6, 0.0);
request.bumps_.directions_(0, 3) = 1.0;
request.bumps_.steps_ = {2e-4};
const auto plan = PlanDupireScriptCurvature(product, model, calibration,
                                           "Z_LOCAL", request);
const auto result = ValueByMonteCarloWithDupireCurvature(plan);
```

## Errors and compatibility

Existing first-order calls retain behavior. New rebuild/curvature calls reject
unsupported external direct seeds, exercise, and curvature recording caps;
they must never silently freeze unknown direct or policy derivatives. Errors
name `DupireScriptCurvature` or `RecalibrateDupireScriptRisk`, phase, and
direction/sign where applicable. Generic invalid bump details remain visible.
Numeric budgets count doubles, not full process memory. Python/Excel exposure
belongs to the later binding stage, with the same request/result semantics.

Rejected alternatives: nesting parallel valuation inside a native objective;
reusing base calibration seeds/Jacobian at bumped quotes; accepting arbitrary
mutable model/product rebuilding callbacks; unstructured payoff-text replacement;
claiming a caller-thread recording cap limits worker tapes.

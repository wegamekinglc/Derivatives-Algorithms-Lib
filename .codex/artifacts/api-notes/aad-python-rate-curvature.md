# Python rate curvature API

Audience: Python callers already using native rate specifications and trades.
The controlling [specification](../specs/aad-python-rate-curvature.md) projects
`dal-public/src/ratecurvature.hpp` without a new differentiable core.

## Surface

- `RateCalibration_New(spec)` has four typed overloads and returns
  `RateCalibrationSnapshot_` (`point`, `parameters`, `provenance`).
- `RateCalibration_Recalibrate(calibration, quotes)` returns a new snapshot;
  it does not mutate the source or reorder its quote axis.
- `RateTradeQuoteCurvatureSettings_(*, weights=None, fixings=None)` exposes
  detached read-only `weights` and an immutable typed `fixings` handle.
- `RateTradeQuoteCurvature(trades, calibration, bumps, *, settings=None)` returns
  `RateTradeQuoteCurvatureResult_` with `currency` and `curvature`.
- `RateQuoteCurvatureResult_`: `value`, `gradient`, `point`, `directions`, `steps`,
  `hessian_products`, `base_calibration`, `execution`.
- `RateQuoteCurvatureExecution_`: `method`, `quote_gradient_evaluations`,
  `calibrations`, `objective_reverse_sweeps`, `numeric_payload_bytes`,
  `peak_tape_bytes`, `cleanup_reserve_bytes`.

All new values support copy/deepcopy. Constructors are passive; result and
snapshot values come from native financial factories. Reuse the one registered
`BumpOverAADRequest_` from #529; keep required arguments first and settings last.

```python
calibration = dal.RateCalibration_New(spec)
directions = dal.DoubleMatrix_(1, len(calibration.point), 0.0)
directions[0, 0] = 1.0
bumps = dal.BumpOverAADRequest_(directions=directions, steps=[0.0002])
result = dal.RateTradeQuoteCurvature([trade], calibration, bumps)
products = result.curvature.hessian_products.to_rows()
```

Raw quotes use native decimal units and provenance order. One direction is one
row; signed weights/directions retain their signs. The finite-step method label
and execution counts accompany the result. This surface accepts existing closed
native financial trades instead of an active Python objective or generic callback.

## Errors and ownership

Wrong object types identify `spec`, `trades[i]`, `calibration`, `bumps`, `quotes`,
`weights`, `fixings` or `settings`. Quotes and weights require copied list/tuple
real values, excluding bool/enums and implicit conversion. Native admission owns
quote count/dimensions, exact-square support, fixing conflicts, trade currencies,
curve eligibility and budget errors. Zero optional budgets remain zero.

Copy every Python-owned value/handle before releasing the GIL; native financial
work runs without Python callbacks. Return vectors, matrices and nested values
by value. A snapshot may share only its native immutable captured data. Existing
first-order pricing, calibration and shared bump semantics remain unchanged.

Rejected alternatives: expose an arbitrary scalar callback (active Python state),
duplicate the bump registration (type conflicts), or implement finite differences
in Python (different admission, provenance and ownership semantics).

Open questions: none.

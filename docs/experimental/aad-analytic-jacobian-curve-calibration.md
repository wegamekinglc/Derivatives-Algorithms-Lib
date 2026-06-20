# AAD Analytic Jacobian for Curve Calibration

> Status: experimental enhancement, shipped and runtime-opt-in.

The curve-calibration residual function overrides
`Underdetermined::Function_::Gradient` (`dal-cpp/dal/curve/calibration.cpp`) with an
AAD-derived analytic Jacobian. It is **off by default** and engaged by a runtime flag.

**Runtime flag.** `CurveJacobianMode_` is a Machinist-generated switchable enumeration
defined in `dal-cpp/dal/curve/calibration.hpp` with two values:

- `CurveJacobianMode_::Value_::BUMPED` (default) — finite-difference bumping of each free
  node, byte-for-byte identical to the pre-analytic path. Always available.
- `CurveJacobianMode_::Value_::ANALYTIC` — the AAD-derived dense Jacobian. Best-effort:
  engages only when `EligibleForAnalyticJacobian()` is true, otherwise falls back to
  `BUMPED` with a `NOTICE`. `ANALYTIC` never throws.

The flag lives on `CurveCalibrationOptions_` — a struct deliberately kept **separate**
from the serialized `CurveCalibrationSpec_`, because the spec describes *what* to
calibrate and the options describe *how* to solve. The default-constructed options
reproduce the pre-analytic bumped path byte-for-byte, so existing callers see no change
unless they opt in:

```cpp
CurveCalibrationOptions_ opt;
opt.jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
CalibrateYieldCurve(spec, opt);
```

**Eligibility.** `EligibleForAnalyticJacobian()` admits a calibration only when **all** of
the following hold (each failing condition emits a `NOTICE` naming it, then the path falls
back to `BUMPED`):

- `parameterization_ == CurveParameterization_::Value_::LOG_DISCOUNT` — the independents
  are the free-node `log(DF)` values, so the Jacobian is taken w.r.t. log-discount nodes;
- the calibration targets the **discount** curve (`calibrateDiscountCurve_`), not a
  forecast/projection curve;
- `forecast == discount` (no separate forecast curve layered in);
- every instrument is a vanilla `Deposit_`, `FRA_`, `Future_`, or `Swap_` (instruments
  without a templated rate, e.g. `BasisSwap_`, are rejected);
- every instrument's **trade date** equals the curve anchor (`knotDates_.front()`).

The trade-date check uses the pure-virtual `YCInstrument_::TradeDate()` accessor, not the
instrument's effective/spot `start_`. A spot-started instrument has `tradeDate` strictly
before its `start_` (the typical spot-lag gap); checking `start_` instead would admit
spot-started instruments and silently misprice their residual rows on the tape.

The eligibility verdict is evaluated once per `CalibrateYieldCurve` call and cached, so
the `NOTICE`s fire at most once even though `Gradient` is invoked per solver iteration.

**Backend neutrality.** The analytic path runs unchanged under **all four** AAD backends
(native, XAD, CoDiPack, Adept). It goes through the `Dal::AAD` facade primitives
(`RegisterIndependent`, `NewRecording`, `ZeroAdjoints`, `Adjoint`,
`PropagateToStart`) rather than any backend-specific API, and there is no longer a
compile-time backend `#if` gate around it in `dal-cpp/dal/curve/calibration.cpp`. The
recording contract that works on every backend is
`Clear(*Tape())` → `RegisterIndependent` → `NewRecording` → forward → per-row
`ZeroAdjoints`/seed/`PropagateToStart`.

**Mechanics.** `YieldCurveCalibrationFunc_::AnalyticJacobian` registers the free-node
`log(DF)` values as independents (the anchor node 0 is pinned at `0` and deliberately
*not* registered, so the solver's `x` has `nNodes - 1` entries), builds a
`Tape::DiscountCurve_<Number_>` via `BuildDiscountCurveT<Number_>`, computes
`Number_`-typed residuals with `PhaseARateAt<Number_>()`, and runs one reverse sweep per
residual row to harvest `∂residual_i / ∂node_j` into a dense `XCurveJacobian_`. Assembly
is sparse by row — AAD produces exact structural zeros at nodes an instrument does not
touch. The multi-result fast path (one sweep for all rows) is a profiling-driven
follow-up; the single-result loop is what ships today.

**Files.** `dal-cpp/dal/curve/{yclogdf,ycinstrument,ycctx,calibration}.{hpp,cpp}`. The
dense forward Jacobian is exposed ONLY as a byproduct of calibration on the public
`CurveCalibrationDiagnostics_::jacobian_` field (populated by `CalibrateYieldCurve` when
`ANALYTIC && EXACT && eligible`); there is no standalone "analytic J at a point" accessor.

**Tests.** `dal-cpp/tests/curve/test_analytic_jacobian.cpp` (suite `AnalyticJacobianTest`)
runs on every backend: central-difference agreement across all three `LogDfScheme_`
values (`LOG_LINEAR`, `LOG_CUBIC_NATURAL`, `MIXED`), exact structural zeros, solve
convergence, per-instrument-type canaries (Deposit/FRA/Future/Swap), tape isolation
across calls, and the eligibility regressions (non-`LOG_DISCOUNT`, forecast-target,
`tradeDate != start`). The flag's own behaviour (default == `BUMPED`, eligible
`ANALYTIC` matches `BUMPED`, ineligible `ANALYTIC` falls back, eligibility is cached) is
covered by `dal-cpp/tests/curve/test_curve_jacobian_mode{,_flag}.cpp`.


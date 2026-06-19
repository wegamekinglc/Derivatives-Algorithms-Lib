# AAD Analytic Jacobian for Curve Calibration

> Status: experimental enhancement, shipped and runtime-opt-in. This note was originally
> a plan extracted from
> [`replicate-ptirds-single-currency-curve.md`](replicate-ptirds-single-currency-curve.md)
> (§3.5 and Phase 6). The implementation went through several revisions; the
> **Current state** section below is authoritative and the historical plan is preserved
> further down for context.

## Current state (2026-06)

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

## How this design was reached

After critic and api-designer review, two designs were on the table:

- **Templatize the curve rebuild + repricing on `Dal::AAD::Number_`** so the AAD tape
  produces exact adjoints, and override `Function_::Gradient` to harvest them. This is
  the path that shipped.
- **A plain-`double` chain-rule Jacobian** (`InterpBasisWeights`, `DRateDDiscount`) with
  no templatization and no `Number_` tape, proposed as a lighter fallback.

The templatization was tractable: the pricing path was generalized on the scalar type via
a parallel `Dal::Tape` namespace (`Tape::DiscountCurve_<T_>`, `Tape::Rate_<T_>`, and
`PrecomputeT<T_>()` / `PhaseARateAt<T_>()` factories on `Deposit_`/`FRA_`/`Future_`/`Swap_`),
so a single `Number_`-typed recording yields the exact residual sensitivities. The
plain-`double` chain-rule machinery was explored then dropped as redundant
(commit `2b2de93`).

A runtime on/off selector was subsequently re-introduced as the `CurveJacobianMode_`
flag (PR #111) so callers can A/B the analytic path against the bumped path and so the
default build is unchanged. The earlier `Phase A` symbol naming was renamed to
`AnalyticJacobian` / `EligibleForAnalyticJacobian` (PR #108) — `Phase A` survives only as
legacy source comments.

The original Phase A/B/C plan is preserved below as historical context.


## 1. Motivation

DAL ships a full reverse-mode Automatic Adjoint Differentiation (AAD) type `Number_`
(`dal-cpp/dal/math/aad/expr.hpp:471`) backed by a tape (`dal-cpp/dal/math/aad/tape.hpp`),
but it is **not** used in curve construction or calibration today (no AAD symbols appear
under `dal-cpp/dal/curve/`, verified by grep). The curve-calibration Jacobian is computed
by finite-difference bumping instead. This is the headroom where DAL can supply an
analytic Jacobian and beat a bumped/auto-diff reference solve.

## 2. Current State (baseline)

- **The solve is global over all knots simultaneously**, not a sequential bootstrap:
  `YieldCurveCalibrationFunc_::F` rebuilds the whole curve and returns
  `modelRate - marketRate` for every instrument at once
  (`YieldCurveCalibrationFunc_::F` in `dal-cpp/dal/curve/calibration.cpp`), handed to
  `Underdetermined::Find` (the `Underdetermined::Find` call in `CalibrateYieldCurve`,
  `dal-cpp/dal/curve/calibration.cpp`).
- **The solver** is the underdetermined least-change search
  (`dal-cpp/dal/math/optimization/underdetermined.hpp:74-86`,
  `docs/methodology/underdetermined_search.md`), supporting an EXACT mode and an
  APPROXIMATE least-squares mode.
- **The Jacobian is finite-difference bumped, not analytic.** The base
  `Function_::Gradient` bumps each parameter by `BumpSize() = 1e-4`
  (`dal-cpp/dal/math/optimization/underdetermined.hpp:60`,
  `dal-cpp/dal/math/optimization/underdetermined.cpp:22-35`).
  `YieldCurveCalibrationFunc_` does **not** override `Gradient`, so curve calibration
  uses bumping today.

| Capability                              | Today | Evidence                                                        | Gap to close                                                            |
|-----------------------------------------|-------|-----------------------------------------------------------------|------------------------------------------------------------------------|
| Reverse-mode AAD type (`Number_`)       | yes   | `dal-cpp/dal/math/aad/expr.hpp:471`, tape `aad/tape.hpp`         | reuse, do not rebuild                                                   |
| AAD used in `dal/curve/`                | no    | no AAD symbols under `dal-cpp/dal/curve/` (grep)                 | wire `Number_` through curve rebuild + repricing                       |
| Analytic Jacobian for the solver        | no    | `Function_::Gradient` bumps (`underdetermined.cpp:22-35`)       | override `Gradient` with an AAD-derived sparse Jacobian                 |

## 3. Proposed Change

Override `Function_::Gradient` in the curve-calibration function with an AAD-derived
**sparse** Jacobian using `Number_` (`aad/expr.hpp:471`) instead of the default bump
(`underdetermined.cpp:22-35`). For a swap repricing, each instrument depends on only a
handful of node `log(DF)` unknowns, so the Jacobian is sparse and AAD delivers it
exactly in one reverse sweep — fewer iterations, no bump noise, exact curve risk.

## 4. Implementation Phases

### Phase A — Templatize the curve rebuild + repricing on the scalar type
- Generalize the calibration residual path (`YieldCurveCalibrationFunc_::F` and the curve
  rebuild it drives, in `dal-cpp/dal/curve/calibration.cpp`) so the node values and
  discount factors can be either `double` or `Number_`. Follow the existing AAD conventions
  (`dal-cpp/dal/math/aad/`): `Clear(*Tape())` → set independents → `NewRecording` →
  compute residuals → `PropagateToStart` → read adjoints.

### Phase B — Override `Gradient` (shipped)
- `YieldCurveCalibrationFunc_::Gradient` overrides the base bumped path
  (`dal-cpp/dal/math/optimization/underdetermined.cpp`). It records the residual vector
  as functions of the node `log(DF)` independents on the tape, runs one reverse sweep
  per row (`AnalyticJacobian`), and writes the resulting
  `∂residual_i / ∂node_j` into a dense `XCurveJacobian_` (storage dense, assembly
  sparse-by-row because AAD produces exact structural zeros).
- Sparsity is automatic: AAD yields exact zeros at nodes an instrument does not touch.

### Phase C — Selection (shipped as a runtime flag + eligibility gating)
- The analytic path is gated at runtime by the `CurveJacobianMode_` flag
  (`CurveCalibrationOptions_::jacobianMode_`, default `BUMPED`) and, when `ANALYTIC` is
  selected, by `EligibleForAnalyticJacobian()` (LOG_DISCOUNT only, discount-target, no
  projection curve, supported instrument type, and `TradeDate() == knotDates_.front()`).
  An ineligible calibration emits a `NOTICE` and the solver dense-bumps unchanged. See
  the **Current state** section above for the full contract.
- The flag is backend-neutral: there is no compile-time backend `#if` gate around the
  analytic path in `dal-cpp/dal/curve/calibration.cpp`; it runs on native, XAD, CoDiPack,
  and Adept via the `Dal::AAD` facade.

## 5. Tests

- **Correctness:** for a representative calibration set, assert the AAD Jacobian matches
  the finite-difference bumped Jacobian column-by-column within a tolerance well above
  bump noise (e.g. `ASSERT_NEAR(..., 1e-8)` on the agreeing entries; looser where bump
  noise dominates).
- **Solve equivalence:** assert the AAD-Jacobian solve converges to the same node DFs as
  the bumped solve within solver tolerance (residual `< 1e-8`) and in no more iterations.
- **Sparsity:** assert structurally-zero Jacobian entries (nodes an instrument does not
  depend on) are exactly zero under AAD.
- Follow the project unit-test conventions (`.claude/rules/unit-test-style.md`):
  `TEST(Suite, Name)`, `ASSERT_*` over `EXPECT_*`, and the AAD setup/teardown pattern
  (`Clear(*Tape())` → `NewRecording` → compute → `PropagateToStart`).

## 6. Risks / Open Questions

- **Tape lifetime / threading.** The tape is thread-local (`Tape()`); the calibration
  solve must record and propagate within a well-scoped RAII region and must not leak tape
  state across solver iterations or threads (`dal-cpp/dal/concurrency/`).
- **Parameterization.** The Jacobian is taken w.r.t. the `log(DF)` node unknowns; the seed
  and the independents must match the parameterization the solver drives.
- **Performance.** AAD adds per-iteration overhead vs a single bump; the win comes from
  exactness and fewer iterations. Benchmark against the bumped path (sibling of
  `dal-cpp/benchmarks/`) before claiming an improvement.
- **Backend availability.** The native AAD backend is always available; the analytic
  path must not assume an optional third-party backend (XAD/CoDiPack/Adept) is enabled,
  since `CMakePresets.json` disables all of them by default.

## 7. Suggested Follow-up Agents / Sequence

1. **`dal-critic`** — stress-test this plan (tape scoping, parameterization, perf claim)
   before any code is written.
2. **`dal-api-designer`** — design the opt-in selector for bumped vs AAD Jacobian.
3. **`dal-implementer`** — implement Phases A–C in an isolated worktree.
4. **`dal-tester`** — write the Jacobian-correctness and solve-equivalence tests.
5. **`dal-reviewer`** — review against coding, unit-test, and documentation conventions
   before merge.

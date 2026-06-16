# Plan — AAD Analytic Jacobian for Curve Calibration

> Status: experimental / proposal. This is an optional enhancement extracted from
> [`replicate-ptirds-single-currency-curve.md`](replicate-ptirds-single-currency-curve.md)
> (§3.5 and Phase 6), where it appears as a follow-on phase. This document promotes it
> to a standalone, self-contained plan.

## Resolution adopted (Phase A — native-AAD templatization, implemented 2026-06)

After critic and api-designer review, two designs were on the table:

- **Phase A** — templatize the curve rebuild + repricing on `Dal::AAD::Number_` so the
  native AAD tape produces exact adjoints, and override `Function_::Gradient` to harvest
  them. Originally scoped as the heaviest of three phases.
- **Counter-Proposal CP1** — an analytic chain-rule Jacobian computed in plain `double`
  (`InterpBasisWeights`, `DRateDDiscount`, a `CurveJacobianMode_` enum), with NO
  templatization and NO `Number_` tape. Proposed as a lighter fallback if Phase A turned
  out infeasible.

**Phase A was adopted.** The templatization turned out to be tractable: the pricing path
was generalized on the scalar type via a parallel `Dal::Tape` namespace
(`Tape::DiscountCurve_<T_>`, `Tape::Rate_<T_>`, and `PrecomputeT<T_>()` factories on
`Deposit_`/`FRA_`/`Future_`/`Swap_`), so a single `Number_`-typed recording yields the
exact residual sensitivities in one reverse sweep per row — no bump noise, exact curve
risk. CP1 was **explored then dropped** (commit `2b2de93`): with Phase A delivering exact
adjoints straight from the tape, the plain-`double` chain-rule machinery
(`InterpBasisWeights`, `DRateDDiscount`, and the `CurveJacobianMode_` opt-in enum) was
redundant and was removed. No CP1 symbols remain under `dal-cpp/dal/curve/` (verified
by grep).

**Scope and gating.** The Phase A Jacobian ships for
`CurveParameterization_::LOG_DISCOUNT` only; other parameterizations, FORECAST-target
calibrations, projection-curve instruments, instruments without a templated rate
(`BasisSwap_`), and — critically — any instrument whose **trade date** differs from the
curve anchor, make `EligibleForPhaseA` return false, `Gradient` returns `nullptr`, and the
solver dense-bumps. A `NOTICE` is emitted per fall-through. The whole path is compiled
only under the native AAD backend, gated by:

```cpp
#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
```

(`dal-cpp/dal/curve/calibration.cpp`). The native backend is the default;
`CMakePresets.json` disables XAD/CodiPack/Adept, so the Phase A path compiles in the
standard build. Under a third-party backend the override is absent and `Gradient` falls
back to the base class's bumped path.

**Eligibility checks the trade date, not the start.** Phase A's templated rates read
`DF(tradeDate_, p)` (see `dal-cpp/dal/curve/ycinstrument.cpp`), so the gate compares
`inst->TradeDate()` against `knotDates_.front()`. A spot-started instrument has
`tradeDate` strictly before its effective/spot `start_` (the typical `spotLag` gap); the
original gate mistakenly checked `TimeSpan().first` (== `start_`), which admitted
spot-started instruments and silently mispriced their residual rows on the tape. The
`YCInstrument_::TradeDate()` pure-virtual accessor (overridden on every concrete
instrument) fixes this.

**Implementation.** `dal-cpp/dal/curve/{yclogdf,ycinstrument,calibration}.{hpp,cpp}`;
the `TestOnly::AnalyticJacobianAt` helper in `calibration.cpp` exposes the dense
`XCurveJacobian_` for unit tests. Verified by
`dal-cpp/tests/curve/test_phase_a_jacobian.cpp` (suite `PhaseAAADJacobianTest`, 12 tests):
central-difference agreement across all three `LogDfScheme_` values, exact structural
zeros, solve convergence, per-instrument-type canaries (Deposit/FRA/Future/Swap), tape
isolation across calls, and the eligibility regressions (non-LOG_DISCOUNT,
FORECAST-target, `tradeDate != start`).

The original Phase A/B/C plan is preserved below as the adopted design.


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
  (`dal-cpp/dal/curve/calibration.cpp:221-235`), handed to `Underdetermined::Find`
  (`dal-cpp/dal/curve/calibration.cpp:375-377`).
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
  rebuild it drives, `calibration.cpp:221-235`) so the node values and discount factors
  can be either `double` or `Number_`. Follow the existing AAD conventions
  (`dal-cpp/dal/math/aad/`): `Clear(*Tape())` → set independents → `NewRecording` →
  compute residuals → `PropagateToStart` → read adjoints.

### Phase B — Override `Gradient` (shipped)
- `YieldCurveCalibrationFunc_::Gradient` overrides the base bumped path
  (`dal-cpp/dal/math/optimization/underdetermined.cpp`). It records the residual vector
  as functions of the node `log(DF)` independents on the tape, runs one reverse sweep
  per row (`PhaseAJacobian_NativeAAD`), and writes the resulting
  `∂residual_i / ∂node_j` into a dense `XCurveJacobian_` (storage dense, assembly
  sparse-by-row because AAD produces exact structural zeros).
- Sparsity is automatic: AAD yields exact zeros at nodes an instrument does not touch.

### Phase C — Selection (shipped as compile-time + eligibility gating, not a runtime enum)
- The analytic path is gated at compile time by the native-backend macro
  (`#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)`)
  and at runtime by `EligibleForPhaseA` (LOG_DISCOUNT only, discount-target, no projection
  curve, supported instrument type, and `TradeDate() == knotDates_.front()`). Anything
  ineligible returns `nullptr` from `Gradient`, and the solver dense-bumps unchanged.
- The originally-proposed runtime `CurveJacobianMode_` opt-in enum was part of CP1 and
  was dropped with it (commit `2b2de93`); there is no runtime selector.

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

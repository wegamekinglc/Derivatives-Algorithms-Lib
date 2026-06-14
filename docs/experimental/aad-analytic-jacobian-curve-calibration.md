# Plan — AAD Analytic Jacobian for Curve Calibration

> Status: experimental / proposal. This is an optional enhancement extracted from
> [`replicate-ptirds-single-currency-curve.md`](replicate-ptirds-single-currency-curve.md)
> (§3.5 and Phase 6), where it appears as a follow-on phase. This document promotes it
> to a standalone, self-contained plan.

## Resolution / Approach adopted (CP1, implemented 2026-06)

The original plan below proposed Phases A (templatize the pricing path on `Number_`),
B (override `Gradient`), and C (opt-in selector). After critic and api-designer review,
**Phase A was dropped entirely** and Phases B/C were reworked into **Counter-Proposal
CP1**: an analytic chain-rule Jacobian computed in plain `double`, with NO templatization
of the pricing path and NO use of the AAD `Number_` tape.

Why: the pricing path is hard-bound to `double` through three abstract interfaces
(`DiscountCurve_::operator()`, `YCInstrument_::Rate_::operator()`, and the concrete rate
classes in `ycinstrument.cpp`). Templatizing the entire stack to admit `Number_` would be
multi-week work (the critic's finding B1), would couple the calibration to a specific AAD
backend (finding S1), and would be disproportionate to the goal of faster, more-exact
LOG_DISCOUNT calibration.

Instead, CP1 factors the Jacobian as:

```
J[i,j] = dResidual_i / dx_j
       = sum_{t in cashflows(i)}  (dRate_i / dDF(t))  *  DF(t)  *  b_j(t)
```

where `x` is the free-node log-DF vector (`NX() = nNodes - 1`, anchor pinned), `b_j(t)`
is the interpolation basis weight at solver column `j` for year-fraction `t` (returned by
`DiscountLogDF_::InterpBasisWeights`), and `dRate_i/dDF(t)` is computed analytically from
the existing `double`-typed rate classes (returned by `YCInstrument_::Rate_::DRateDDiscount`).
The four concrete rate classes override `DRateDDiscount` analytically via the quotient rule;
the default empty return triggers a per-instrument DF-bump fallback (in `double`, narrow).

Scope: CP1 ships the analytic Jacobian for `CurveParameterization_::LOG_DISCOUNT` only.
Other parameterizations silently fall back to the bumped path with a `NOTICE`. A new
`CurveJacobianMode_` Machinist enum (`BUMPED` default, `ANALYTIC_LOG_DISCOUNT` opt-in)
selects the path; default `BUMPED` is byte-for-byte unchanged from pre-CP1 behaviour.

The implementation lives in `dal-cpp/dal/curve/{yclogdf,ycinstrument,calibration}.{hpp,cpp}`
and is verified by three test categories under `dal-cpp/tests/curve/test_analytic_jacobian.cpp`
plus per-component tests under `test_interpbasis.cpp` and `test_drate_ddiscount.cpp`.

The original Phase A/B/C plan is preserved below for context.

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

### Phase B — Override `Gradient`
- Add a `Gradient` override on `YieldCurveCalibrationFunc_` that records the residual
  vector as functions of the node `log(DF)` independents on the tape, runs the reverse
  sweep, and writes the resulting `∂residual_i / ∂node_j` into the Jacobian the solver
  expects (`underdetermined.hpp:60`, `underdetermined.cpp:22-35`).
- Exploit sparsity: only populate entries for the nodes an instrument actually touches.

### Phase C — Wire selection
- Provide a calibration option to choose **bumped** (default, unchanged) vs **AAD**
  Jacobian so the analytic path is opt-in and the existing behavior is preserved.

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

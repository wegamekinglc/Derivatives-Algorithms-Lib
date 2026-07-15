# Replicating the PTIRDS Single-Currency Curve in DAL

> Status: validated replication note. The three curve shapes are covered by the
> current test suite; this note records the external benchmark and DAL mapping.

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
  a configurable interpolation rule (DAL's `LOG_DISCOUNT` parameterization via
  `NewDiscountLogDF`), alongside the forward-rate parameterizations DAL also ships.
- **Interpolation** — log-linear on DF, cubic spline on log-DF, and a piecewise
  "mixed" scheme, all selected by `LogDfScheme_`.
- **Calibration** — a single global least-squares/least-change solve over all curve
  nodes simultaneously, exactly the regime DAL's underdetermined search targets.
- **Date machinery** — IMM-style node dates, single-business-day and stub swaps,
  Act/365F, an external every-day calendar reproduced with explicit-date `Unadjusted`
  conventions, annual frequency, and zero payment lag.
- **AAD** — eligible `LOG_DISCOUNT` calibration uses an AAD-derived analytic Jacobian.

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
- Concrete builders:
  - `NewDiscountPWLF` — piecewise-**linear forward** over knot dates
    (`dal-cpp/dal/curve/ycimp.hpp`), backed by `PiecewiseLinear_`
    (`dal-cpp/dal/curve/piecewiselinear.hpp`, which integrates a forward rate
    to produce `log(DF)`).
  - `NewDiscountPWC` — piecewise-**constant forward** (`dal-cpp/dal/curve/ycconst.hpp`).
  - `NewDiscountLogDF` — the DF-node parameterization this exercise needs: a curve
    defined by explicit node dates + `log(DF)` values with a pluggable
    `LogDfScheme_` interpolation rule on `log(DF)`
    (`dal-cpp/dal/curve/yclogdf.hpp`, `dal-cpp/dal/curve/yclogdf.cpp`; see
    [Log-discount curve](../methodology/log_discount_curve.md)). It is selected by
    the `LOG_DISCOUNT` value of `CurveParameterization_`.
  - `NewDiscountZeroRate` — a persistent continuously compounded zero-rate curve.
    Future rates map to log-DF nodes and then use the same `LogDfScheme_` machinery
    (`dal-cpp/dal/curve/yczerorate.hpp`). This representation is not needed to match
    the published DF-node table, but is available in the same calibration framework.

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
- **On-curve schemes.** The `LogDfScheme_` enumeration
  (`dal-cpp/dal/curve/logdfscheme.hpp`) selects how `DiscountLogDF_` interpolates
  between node `log(DF)` values, covering all three reference schemes:
  `LOG_LINEAR` (linear in $\ell$, scheme 1), `LOG_CUBIC_NATURAL` (natural cubic
  spline in $\ell$, scheme 2), and `MIXED` (linear through a cutoff knot, natural
  cubic beyond, scheme 3). The scheme is carried on
  `CurveCalibrationSpec_::logDfScheme_` and
  dispatched in `dal-cpp/dal/curve/yclogdf.cpp` (see
  [Log-discount curve](../methodology/log_discount_curve.md)). The cubic and mixed
  forms are natural cubics over the value array, not rateslib's clamped B-spline with
  repeated boundary knots — see §2.4 for how the boundary mapping is validated.
- **`CurveParameterization_` status.** `PIECEWISE_CONSTANT_FWD`,
  `PIECEWISE_LINEAR_FWD`, `LOG_DISCOUNT`, and `ZERO_RATE` are all implemented through
  the shared `CurveDefinition_`, `CurveParameterLayout_`, and typed factory. This
  replication selects `LOG_DISCOUNT` because its numerical oracle consists of free
  log-DF nodes.

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
- **Jacobian.** `YieldCurveCalibrationFunc_` overrides
  `Underdetermined::Function_::Gradient` to supply an AAD-derived analytic Jacobian
  for every implemented parameterization when the remaining gates are met
  (discount-target, `forecast == discount`, and every instrument trades at the curve
  anchor). Ineligible specs fall back to the base finite-difference bump
  (`BumpSize() = 1e-4`, `dal-cpp/dal/math/optimization/underdetermined.cpp`). The
  eligibility verdict is evaluated once per `CalibrateYieldCurve` call and cached;
  see [Yield-curve Jacobian](../methodology/yield_curve_jacobian.md).

### 3.4 Day count / calendar / schedule / payment lag

- **Act/365F** is a first-class `DayBasis_` alternative (`ACT_365F`),
  in `dal-cpp/dal/time/daybasis.hpp`.
- **Every-day target calendar:** the external target treats every day as a business
  day. DAL reproduces the supplied explicit dates with `Holidays::None()` plus
  `Unadjusted` accrual, payment, and index conventions. `Holidays::None()` removes
  named holidays, but weekends remain non-business days unless date adjustment is
  bypassed (`dal-cpp/dal/time/holidays.hpp`,
  `dal-cpp/dal/curve/ycinstrument.cpp`).
- **Annual frequency:** `PeriodLength_("12M")`; swap leg conventions accept it
  (`dal-cpp/dal/curve/ycinstrument.cpp`).
- **Payment lag:** `RateLegConvention_::paymentLag_` flows through
  `BuildLegPeriods` → `MakeSchedulePeriods` (`dal-cpp/dal/curve/ycinstrument.cpp`);
  lag 0 is expressible.
- **IMM / stub swaps:** `Swap_` has a constructor taking explicit
  `(tradeDate, start, maturity, ...)` with arbitrary dates and per-leg conventions
  (`dal-cpp/dal/curve/ycinstrument.hpp`). Schedules are generated forward with
  configurable stubs (`SchedulePeriod_::isStub_`, `dal-cpp/dal/time/schedules.hpp`).
  Explicit effective/termination IMM stub swaps are therefore constructible, and a
  single very short (1-business-day) swap is just a degenerate single-period swap.

### 3.5 AAD

- DAL ships a full reverse-mode AAD type `Number_`
  (`dal-cpp/dal/math/aad/expr.hpp`) with tape (`dal-cpp/dal/math/aad/tape.hpp`).
- It **is** used in curve calibration: `YieldCurveCalibrationFunc_::Gradient`
  produces an AAD-derived analytic Jacobian on eligible PWC, PWL, LOG_DISCOUNT, and
  ZERO_RATE specs (§3.3), with a bumped fallback when the eligibility predicate rejects
  the spec.

### 3.6 Public API / Python bindings / examples

- **Public C++ facade** (`dal-public/src/`) exposes curve conventions and instrument
  builders, calibration spec builders, `CalibrateSingleCurve`,
  `CalibrateMultiCurveBundle`, and `CalibrateXccyMarket`. It remains a convenience
  facade over concrete core types rather than an ABI-isolated boundary; see the
  [public API guide](../public-api.md).
- **Python bindings** (pybind11, `dal-python/src/bindings/`) wire `core`, `global`,
  `models`, `random`, `script`, `value`, plus `curve` and `calendar`
  (`dal-python/src/bindings/module.cpp`). The `curve` translation unit
  (`dal-python/src/bindings/curve.cpp`) exposes the instrument builders
  (`Deposit_New`, `FRA_New`, `Future_New`, `Swap_New`, `OISSwap_New`,
  `BasisSwap_New`, `CrossCurrencySwap_New`), the `DiscountPWLF_New` and
  `DiscountZeroRate_New` curve factories,
  the calibration entry points
  (`CalibrateSingleCurve`, `CalibrateMultiCurveBundle`, `CalibrateXccyMarket`),
  the `CurveParameterization` / `CurveSolveMode` / `CurveJacobianMode` /
  `LogDfScheme` enums, and `CurveCalibrationSpecBuilder_`; `dal-python/src/dal/api.py`
  adds a `calibrate_curve(...)` convenience wrapper.
- **Examples:** `dal-cpp/examples/curve_calibration/curve_calibration.cpp` already
  demonstrates `CurveCalibrationSpec_` / `MultiCurveCalibrationSpec_` with
  `Holidays::None`, explicit knot dates, and `CalibrateMultiCurve`.

## 4. Capability Inventory

Each row maps a capability the reference exercise needs onto its current DAL
implementation; all required items are available or have an explicit DAL analogue.

| Capability                                    | Status   | Evidence (path)                                                                                       | Notes                                                                                  |
|-----------------------------------------------|----------|-------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
| Discount curve interface (DF from/to)         | yes      | `dal-cpp/dal/curve/discount.hpp`                                                                      | reuse as-is                                                                            |
| DF-node curve + `log(DF)` interpolation rule  | yes      | `NewDiscountLogDF`, `dal-cpp/dal/curve/yclogdf.hpp` / `yclogdf.cpp`                                   | node dates + `log(DF)` + pluggable `LogDfScheme_`; selected by `CurveParameterization_::LOG_DISCOUNT` |
| Log-linear on DF (scheme 1)                   | yes      | `LogDfScheme_::LOG_LINEAR`, `dal-cpp/dal/curve/logdfscheme.hpp`                                       | linear in $\ell$ = log-linear in $P$                                                   |
| Cubic on `log(DF)` (scheme 2)                 | yes      | `LogDfScheme_::LOG_CUBIC_NATURAL`, `dal-cpp/dal/curve/yclogdf.cpp`                                    | natural cubic spline in $\ell$ (Boundary_(2,0.0)), not rateslib's clamped B-spline     |
| "Mixed" (log-linear → log-cubic) (scheme 3)   | yes      | `LogDfScheme_::MIXED`, `dal-cpp/dal/curve/yclogdf.cpp`                                                | linear through a cutoff knot, natural cubic beyond; C0 at the cutoff                   |
| Knot-sequence (`t`) configuration             | n/a      | —                                                                                                     | DAL uses knot dates + scheme, not a B-spline knot vector                               |
| IMM / stub swaps via explicit dates           | yes      | `Swap_(tradeDate, start, maturity, ...)`, `dal-cpp/dal/curve/ycinstrument.hpp`                        | explicit effective/termination dates per leg                                          |
| 1-business-day swap                           | yes      | degenerate single-period `Swap_`, `dal-cpp/dal/curve/ycinstrument.cpp`                                | 1-day span; annuity > 0 path                                                           |
| Act/365F day count                            | yes      | `ACT_365F`, `dal-cpp/dal/time/daybasis.hpp`                                                           | reuse                                                                                  |
| Every-day target calendar                     | yes      | `Holidays::None()` plus `Unadjusted`, `dal-cpp/tests/curve/test_ptirds_curve.cpp`                      | preserves the supplied dates; `Holidays::None()` alone still excludes weekends          |
| Annual frequency, payment lag 0               | yes      | `RateLegConvention_`, `dal-cpp/dal/curve/ycinstrument.cpp`                                            | `PeriodLength_("12M")`, `paymentLag_ = 0`                                              |
| Global simultaneous solve over nodes          | yes      | `Underdetermined::Find`, `dal-cpp/dal/curve/calibration.cpp`                                          | one global solve, not a sequential bootstrap                                           |
| Levenberg-Marquardt least-squares             | analog   | underdetermined least-change EXACT/APPROXIMATE, `dal-cpp/dal/math/optimization/underdetermined.hpp`   | equivalent for the square 13×13 case                                                   |
| Analytic (AAD) Jacobian for the solver        | yes      | `YieldCurveCalibrationFunc_::Gradient`, `dal-cpp/dal/curve/calibration.cpp`                           | AAD reverse sweep when eligible; bumped fallback otherwise                             |
| Anchor node with fixed DF = 1                 | yes      | `LOG_DISCOUNT` anchor exclusion, `dal-cpp/dal/curve/calibration.cpp`                                  | anchor pinned at $\ell_0 = 0$, excluded from unknowns                                  |
| Public API exposure                           | yes      | `dal-public/src/curvespec.hpp`, `dal-python/src/bindings/curve.cpp`                                   | C++ convenience facade and Python builders expose calibration workflows                |
| Python bindings                               | yes      | `dal-python/src/bindings/curve.cpp`                                                                   | instruments, curves, calibration entry points, enums, `CurveCalibrationSpecBuilder_`  |

## 5. Reproduction Pipeline

Reproducing the reference table exercises the as-built pipeline below. The curve,
instrument, solve, AAD, C++ facade, and Python binding paths are all present; the
numerical validation lives in the core C++ test suite.

### 1. DF-node discount curve with pluggable interpolation
- `NewDiscountLogDF` (`dal-cpp/dal/curve/yclogdf.hpp`) is the curve this exercise
  needs: node dates + `log(DF)` values with a `LogDfScheme_` interpolation rule.
  `operator()(from, to)` returns `exp(interp(t_to) - interp(t_from))` in the Act/365F
  year-fraction metric.
- The `LOG_DISCOUNT` branch of `CurveParameterization_` is implemented
  (`dal-cpp/dal/curve/calibration.cpp`), so the global solver drives the node
  `log(DF)` directly.
- Node 0 (`2022-01-01`) is the **fixed anchor** ($\ell_0 = 0$, `DF = 1`), excluded
  from the unknown vector, so the solver has 13 free parameters for 13 instruments
  (square, exactly determined).

### 2. Log-cubic spline parameterization
- `LogDfScheme_::LOG_CUBIC_NATURAL` selects a natural cubic spline in $\ell$ over the
  node values (`Boundary_(2, 0.0)` at both ends), dispatched in
  `dal-cpp/dal/curve/yclogdf.cpp`. This is DAL's scheme-2 analogue; it is a natural
  cubic, not rateslib's clamped B-spline with repeated boundary knots (see §2.4 for
  how the boundary mapping is validated).

### 3. Mixed (composite) interpolator
- `LogDfScheme_::MIXED` is linear through a cutoff knot and natural cubic beyond it, with C0
  continuity at the cutoff (`dal-cpp/dal/curve/yclogdf.cpp`). This is DAL's scheme-3
  analogue.

### 4. Instrument construction for the PTIRDS set
- The 13 swaps are built via the explicit-date `Swap_` constructor
  (`dal-cpp/dal/curve/ycinstrument.hpp`): annual fixed/float legs, Act/365F,
  `Holidays::None()`, `Unadjusted` accrual/payment/index conventions, payment lag 0,
  explicit IMM stub effective/termination dates, and the degenerate 1-business-day
  swap. The external target uses an every-day calendar; the explicit `Unadjusted`
  conventions are required because `Holidays::None()` removes named holidays but
  still treats weekends as non-business days. The stub day-count context
  (`SchedulePeriod_::dayCountContext_`, a `Handle_<DayBasis::Context_>` built by
  `BuildSinglePeriodSchedule` in `dal-cpp/dal/curve/calibration_internal.hpp`)
  reproduces the reference accruals.

### 5. Global solve + verification harness
- A `CurveCalibrationSpec_` with `parameterization_ = LOG_DISCOUNT`,
  `knotPolicy_ = INPUT`, the 14 node dates, the 13 instruments and par rates, and
  `solveMode_ = EXACT` is solved via `CalibrateYieldCurve` for each of the three
  schemes.
- Solved node DFs are compared against the **per-scheme** column of the §2.5 table
  within `1e-6` (each scheme validated against its own rateslib Table 6.2 column,
  **not** against the other two schemes), with repricing residuals `< 1e-8` per
  instrument. The three schemes **deliberately differ** at the nodes (log-cubic
  diverges from log-linear throughout by ~`1.2e-5`; mixed matches log-linear exactly
  through `2025-01-01` and diverges only at `2027/2029/2032`), so cross-scheme
  agreement is not a valid acceptance criterion.

### 6. AAD analytic Jacobian
- `YieldCurveCalibrationFunc_::Gradient` overrides the bumped default with an
  AAD-derived Jacobian (`dal-cpp/dal/curve/calibration.cpp`). The harvester retains
  full width: log-linear rows are locally supported, whereas natural-cubic and mixed-tail
  weights can make a row dense. AAD delivers each row by reverse sweep without bump
  noise, using the same typed interpolation as passive pricing.

### 7. C++ public API facade
- `dal-public/src/curve{protocol,instrument,data,spec}.hpp` exposes the convention,
  instrument, curve, and calibration builders used by this workflow. The facade uses
  core DAL value and handle types directly; it is a convenience API, not an ABI-isolated
  compatibility layer.

### 8. Python bindings
- pybind11 bindings for the curve/calibration surface ship in
  `dal-python/src/bindings/curve.cpp` (registered in `module.cpp`): the instrument
  builders, curve factories, calibration entry points, enums, and
  `CurveCalibrationSpecBuilder_`, with a `calibrate_curve(...)` convenience wrapper
  in `dal-python/src/dal/api.py`. A Python user can reproduce the table directly.

## 6. Validation Surface

- **Tests:** `dal-cpp/tests/curve/test_ptirds_curve.cpp` is the validation harness.
  It asserts:
  - solved node DFs match §2.5 within `1e-6` for `log_linear` (validated against the
    `log-linear` column of rateslib Table 6.2);
  - solved node DFs match §2.5 within `1e-6` for `log_cubic` and `mixed` (each
    validated against its **own** rateslib Table 6.2 column — **not** against
    `log_linear`); the three schemes deliberately differ at the nodes (log-cubic
    diverges from log-linear throughout by ~`1.2e-5`; mixed matches log-linear exactly
    through `2025-01-01` and diverges only at `2027/2029/2032`);
  - each calibrated curve reprices all 13 instruments within the solver tolerance
    (residual `< 1e-8`).
- **Related examples:** the general C++ calibration examples under
  `dal-cpp/examples/curve_calibration/` and the Python calibration surface documented
  in `dal-python/README.md` show the supported entry points. The PTIRDS-specific
  numerical oracle remains the test above.

## 7. Risks / Open Questions

- **Convention ambiguity.** The reference itself notes several conventions are
  *assumed*. The exact roll/stub rule for the IMM dates, the 1-business-day swap's
  termination, and whether the fixed and float legs share the annual schedule must be
  pinned to reproduce the table to 6 dp.
- **Solver equivalence.** DAL's underdetermined least-change EXACT mode is not
  literally Levenberg-Marquardt. For the square 13×13 case it converges to the same
  root, but the smoothing-weight metric (`smoothingWeight_`,
  `BuildCurveCalibrationWeights`, `dal-cpp/dal/curve/calibration.cpp`) must be neutral so it
  does not bias an exactly-determined solve.

The shipped `LOG_DISCOUNT` contract fixes the natural-cubic end conditions, pins
the anchor at $\ell_0 = 0$, and selects a parameterization-specific initial seed;
none of these is an open configuration gap for this replication.

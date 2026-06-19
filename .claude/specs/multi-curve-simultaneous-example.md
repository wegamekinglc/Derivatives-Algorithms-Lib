# Joint Multi-Curve Calibration + Example - Specification

## Amendment (2026-06-20): optional base layering for the joint forward curves

The capability gained an opt-in `JointCurveDeclaration_::baseLayeredOverDiscount_` flag (forward
declarations only). When `true`, the joint forward curve is built as
`NewDiscountPWLF(..., base = the discount curve at targetCollateral_ built in the SAME solve)`, so the
joint smoother acts on the OIS *spread* forward `f_abs - f_ois` - matching the staged path's
`ApplyStageDefaults` base layering. The stored joint forward curve is then structurally identical to
the staged forward curve (`DiscountPWLF_` with `base = OIS`), which **fixes B-new-2 for the opt-in
path**: a bump to the OIS slice now propagates into the joint forward DFs through the `base_` handle
(previously the baseless joint forward had zero OIS sensitivity as a standalone object). The baseless
representation remains the default and is still supported.

The example (`BuildJointSpec`) sets `baseLayeredOverDiscount_ = true` on its 3M declaration. Under
EXACT, the **re-measured** joint-vs-staged DF drift is:

| Bar   | Before (baseless)        | After (base-layered)     | Reference |
|-------|--------------------------|--------------------------|-----------|
| BAR-A | PASS (~0 residual)       | PASS (~5e-9 rate)        | `1e-7` (gate, unchanged) |
| BAR-B | `3.68e-8` (OIS drift)    | `6.42e-7` (OIS drift)    | `1e-6` (informational)   |
| BAR-C | `3.33e-4` (3M drift)     | `2.39e-5` (3M drift)     | `5e-5` (informational)   |

The headline result: **BAR-C drops from `3.33e-4` to `2.39e-5` (~14x tighter)**, and the 2Y-7Y core
agrees to ~`1e-7` (the OIS-agreement level). The remaining drift is NOT round-off: the joint solve
co-optimizes the OIS and 3M-spread knots simultaneously, so its OIS slice lands a few e-7 off staged's
standalone OIS solve, and that OIS difference propagates through the 3M `base_` handle (BAR-B widened
from `3.7e-8` to `6.4e-7` for the same reason). Base layering eliminates the *representation* mismatch
(raw-PWL-absolute vs `DiscountPWLF_`-spread) but cannot eliminate the *optimization-structure*
difference (joint co-optimization vs staged sequential). Both are legitimate, informational drift.

The passages below that assert the baseless representation as the design (the "no base concept"
wording, the BAR-C `3.33e-4` table, the B-new-2 structural caveat, the "raw PWL absolute forward"
framing) describe the **default baseless path**, which remains valid. Where the example is concerned,
read them as superseded by this amendment for the base-layered representation the example now uses.

## Source

- User request (2026-06-19, overridden 2026-06-20): add an example of building multi-curves with
  **joint simultaneous** calibration, validated against the staged path, with a comparison table,
  and instruments totalling **no less than 20**.
- Override decision (2026-06-20): interpretation **(a) - a JOINT simultaneous multi-curve
  calibration** - ONE solve over ALL curves' instruments at once (a single residual system spanning
  the OIS discount curve AND the IBOR forward curve), contrasted with the staged sequential
  `CalibrateMultiCurve`. The prior (b) spec ("per-curve global solve, assembled") is superseded.
  This is a **scope escalation** to a new library capability plus an example.
- Related methodology: `docs/methodology/yield_curve.md` (multi-curve framework, base-curve
  layering, calibration as root-finding, underdetermined smoothness),
  `docs/methodology/xccy_calibration.md` (a second multi-curve solver pattern that already builds a
  multi-curve residual system and hands it to `Underdetermined::Find`),
  `docs/methodology/underdetermined_search.md` (the solver the joint residual feeds).
- Prior artifacts (superseded, retained for traceability):
  - Spec under interpretation (b): this file, prior revision.
  - Critique under interpretation (b): `.claude/critiques/multi-curve-simultaneous-example.md`
    (findings B1/B2/B3 are re-examined below under (a)).

## Problem Statement

The library calibrates multi-curve sets **only sequentially** today. `CalibrateMultiCurve`
(`dal-cpp/dal/curve/calibration.cpp:833-844`) is a per-stage loop: stage N calls
`CalibrateYieldCurve`, then `ApplyStageDefaults` (`calibration.cpp:103-118`) loads the accumulated
discount/forward curves into stage N+1 and, for a forecast-target stage with no `baseCurve_`, sets
`baseCurve_ = discountCurves_.at(targetCollateral_)` (the OIS curve). Each stage is itself a global
simultaneous solve over its own knots via `Underdetermined::Find`, but **no single solve spans the
instruments of two curves at once**. Cross-curve coupling (how an OIS knot perturbation moves a
3M-swap residual through the shared discount curve) is never present inside any one residual system;
it is approximated by feeding stage 1 into stage 2's base.

A user who wants the genuinely coupled answer - one `Underdetermined::Find` over a joint
free-parameter vector with the cross-curve blocks of the Jacobian populated - has no library entry
point. This spec adds that capability and an example that exercises it and contrasts it with the
staged path.

## Goals

- **Add a NEW library capability**: a joint simultaneous multi-curve calibration that builds ONE
  residual vector spanning every instrument across every curve, drives ONE free-parameter vector
  (the concatenation of each curve's free knots) through `Underdetermined::Find`, and returns the
  calibrated curves plus joint diagnostics. The exact public C++ signature is left to
  `dal-api-designer`; this spec describes the behavior and what the example calls.
- **Add an example** that calibrates a two-curve set (OIS discount + 3M IBOR forecast) jointly via
  the new capability and cross-validates against the existing staged `CalibrateMultiCurve`.
- **Construct >= 20 instruments** total (the design uses 24: 12 OIS-curve + 12 IBOR-3M-curve).
- **Print a comparison table** of joint-vs-staged: per-pillar discount factors on the OIS curve, per-
  pillar forward discount factors on the 3M curve, and per-instrument residuals from both paths.
- **Run a self-check** with one hard pass/fail gate (BAR-A: both paths reprice every instrument
  within `1e-7`) that can FAIL if the joint solve is wrong, plus two informational joint-vs-staged
  drift measurements (BAR-B on the OIS curve, BAR-C on the 3M curve) that stay predictive for
  gross-regression detection without throwing.

## Non-Goals

- **No new parameterization.** The joint solve reuses the existing
  `CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD` path; it does not introduce a new curve
  storage type.
- **No AAD / analytic Jacobian for the joint solve.** The joint residual function returns a
  finite-difference (bumped) Jacobian to `Underdetermined::Find`. This mirrors the default
  `CurveJacobianMode_::Value_::BUMPED` path. Extending the AAD analytic-Jacobian machinery
  (currently scoped to single-curve LOG_DISCOUNT, `calibration.cpp:417-438`) to the joint system is
  explicitly out of scope and left to a follow-up.
- **No cross-currency calibration.** Single-currency (USD), OIS discount + 3M forecast only. XCCY
  is covered by `xccy_curve_calibration` and `CalibrateCrossCurrencyMarket`.
- **No more than two curves in the example.** The capability is general (N curves), but the example
  exercises exactly two (OIS + 3M) to keep the output readable. The capability must not bake in a
  two-curve assumption.
- **No real market data.** Synthetic but self-consistent par rates, derived by repricing zero-quote
  prototypes against a flat-curve market - the same `QuotedInstrument` idiom as
  `curve_calibration.cpp:46-87`.
- **No persistence / serialization.** Stdout only. The joint spec and result structs are
  in-memory; JSON/CSV output is out of scope.
- **No performance benchmarking target.** The example prints timings but asserts no latency bar; the
  joint solve is expected to be slower than two sequential solves (larger system per iteration),
  and that is acceptable for a teaching example.

## CRITICAL SCOPE DECISION: Interpretation of "Simultaneous"

**Chosen: interpretation (a) - joint simultaneous multi-curve calibration.**

A single `Underdetermined::Find` call spans the free parameters of every curve at once. The residual
function builds all curves from one parameter vector, routes each instrument's pricing through the
appropriate curve(s), and returns the stacked residuals. Cross-curve coupling is present directly in
the Jacobian: an OIS knot perturbation moves both the OIS residuals and (through the shared
discount curve used to PV the 3M-swap annuity and fixings) the 3M residuals.

This is contrasted with the staged `CalibrateMultiCurve` path, where each stage is a separate
`Underdetermined::Find` and the only inter-stage link is `ApplyStageDefaults` feeding stage 1's OIS
result into stage 2's `baseCurve_`.

**Why (a) over (b):** the user has explicitly overridden the (b) framing. (a) requires a NEW library
capability because no public entry point today builds a joint residual system; the remainder of this
spec defines that capability behaviorally and leaves the exact signature to `dal-api-designer`.

## The Joint Multi-Curve Calibration Capability (behavioral spec)

This section describes WHAT the capability must do. The exact C++ function signature, struct names,
and include paths are `dal-api-designer`'s decision (see the "API placeholder" call-out below). The
example code in this spec refers to the capability by a placeholder name and calls it as described
here; the API step will replace the placeholder with the real signature.

### Joint free-parameter vector

Let the multi-curve set have $C$ curves (in the example, $C=2$: OIS discount and 3M forward). Curve
$c$ has $K_c$ knots and uses `parameterization_ == PIECEWISE_LINEAR_FWD`, which gives
`ParamsPerKnot == 2` (`dal-cpp/dal/curve/calibration.cpp:179-194`). The joint free-parameter vector
is the concatenation

$$
x = \big(x_1,\; x_2,\; \dots,\; x_C\big), \qquad \dim(x_c) = 2 K_c.
$$

No anchor is excluded (PWL does not pin the anchor; `calibration.cpp:659-664` REQUIREs only
`knotDates.front() > today_` for non-LOG_DISCOUNT). For the example's two 9-knot curves,
$\dim(x) = 2 \cdot 9 + 2 \cdot 9 = 36$.

### Joint residual system

Every instrument across every curve contributes one residual. Instrument $j$ on curve $c$ has
residual

$$
r_{c,j}(x) = \text{modelRate}_{c,j}(x) - \text{marketRate}_{c,j},
$$

where `modelRate_{c,j}(x)` is computed by:

1. Build ALL $C$ curves from $x$ (each curve from its slice $x_c$).
2. Assemble them into a `CurveBlock_` so multi-curve routing
   (`curveblock.hpp:28-32`, OIS discount + 3M forecast) is available.
3. Price instrument $(c,j)$ through that `CurveBlock_`, exactly as `YieldCurveCalibrationFunc_::F`
   does today (`calibration.cpp:338-347`).

The joint residual vector is the concatenation $r(x) = (r_{1,\cdot}, r_{2,\cdot}, \dots, r_{C,\cdot})$.
For the example: $12 + 12 = 24$ residuals.

### Routing invariant / precondition (B-new-1)

**INVARIANT: for every IBOR (forward-curve) instrument, the float-leg fixing MUST route
off the forward curve.** Concretely, the instrument's `RateIndexConvention_` MUST have
`useProjectionCurve_ == true` (and `forecastTenor_` equal to the declaration's
`targetTenor_`), so that `ResolveForecastCurve` (`dal-cpp/dal/curve/ycinstrument.cpp:41-51`)
takes the `yc.Forward(3M, OIS)` branch (line 47) and reads the fixing off the joint 3M
forward curve rather than the `yc.Discount(OIS)` fallback branch.

**Why this matters (load-bearing for the example's central teaching payload):**
`RateIndexConvention_::useProjectionCurve_` defaults to `false`
(`dal-cpp/dal/protocol/rateconvention.hpp:17`). With that default, the fixing resolves to
`ResolveDiscountCurve(yc, ...)` (`ycinstrument.cpp:44-45`) and the float leg fixes off the
OIS discount curve, NOT the 3M forward curve. The 3M forward curve is then left completely
unconstrained by any instrument (no residual row reads it), and BAR-C's measured ~`3.3e-4`
agreement between the joint and staged 3M curves collapses from a structural identity into
a coincidence of the smoothing regularizer picking a member of a now much larger
underdetermined manifold. BAR-A would still pass (calibration converges to *something*),
but BAR-C's printed drift would jump to percent-level with no principled way to tell whether
the bug is in the routing or the convention - exactly the silent-failure mode the (a) critique
flags as the most likely implementation blocker.

**How the example satisfies the invariant today:** the 3M Libor index is constructed via
`Ccy::Conventions::LiborIndex()(Ccy_("USD"))`, which inherits the USD Libor convention
initialized in `dal-cpp/dal/currency/init.cpp:36-45` with `useProjectionCurve_ = true`
(line 39) and `forecastTenor_ = "3M"` (line 40). This is the same dependency
`dal-cpp/examples/curve_calibration/curve_calibration.cpp:259` silently relies on (the
explicit assignment to `true` only appears in the unrelated standalone `libor3mIndex` at
`PrintForwardInstrumentExample` line 181). The example does NOT set the field by hand - it
inherits the currency default. **This dependency must be named, not silently relied upon:**
any future maintainer who constructs an IBOR instrument by hand (or copies a Deposit/FRA
from a single-curve test, where `useProjectionCurve_ == false` is typical) will silently
break BAR-C.

**Scope of this spec vs. the capability:** Naming the invariant and its rationale is THIS
spec's job (so it is visible to the implementer and reviewer). Enforcing the invariant in
the capability itself - a validator that REQUIRES every forward-curve declaration's
instruments to carry `useProjectionCurve_ == true` and a matching `forecastTenor_` - is
`dal-api-designer`'s job (a row in the API note's validator table). The example is
responsible for constructing its instruments with the invariant satisfied (via the
`LiborIndex()(USD)` default).

### Solve approach (how it generalizes single-curve `CalibrateYieldCurve`)

The joint capability generalizes the single-curve path in `CalibrateYieldCurve`
(`calibration.cpp:791-831`) along three axes, and is otherwise the same solver:

1. **Parameter vector.** Single-curve builds `x` of length `paramsPerKnot * nFreeKnots` for one
   curve (`calibration.cpp:796-800`). The joint capability builds `x` of length
   $\sum_c \text{paramsPerKnot} \cdot K_c$ spanning all curves.
2. **Residual function.** Single-curve's `YieldCurveCalibrationFunc_::F`
   (`calibration.cpp:282-347`) builds one curve from `x`, slots it into one `CurveBlock_`, and
   returns that curve's residuals. The joint residual function builds all curves from `x`, assembles
   them into one `CurveBlock_`, and returns the stacked residuals of every curve's instruments. The
   IBOR instruments are priced with the OIS curve supplying discounting (the post-2008 routing in
   `curveblock.cpp`), so the OIS slice of `x` enters the IBOR residual rows directly.
3. **Smoothing weight matrix.** Single-curve builds one tridiagonal smoothing operator
   `BuildCurveCalibrationWeights(knotDates, paramsPerKnot, smoothingWeight)`
   (`calibration.cpp:616-626`) over its knots. The joint capability builds a **block-diagonal**
   smoothing matrix: one tridiagonal block per curve, on its own knot grid, zero off-block. This
   penalizes roughness WITHIN each curve and does NOT couple smoothing across curves (the data
   coupling is already in the residual; the smoothing matrix is a regularizer, not a coupling).

The solver call defaults to `Underdetermined::Find` (EXACT) - `solveMode_ == EXACT` is the library
default and is set explicitly at all three call sites in the example (joint spec, OIS stage, 3M stage;
see commit `6305bf5`). `Underdetermined::Approximate`
(`dal-cpp/dal/math/optimization/underdetermined.hpp:74-87`) remains selectable but is no longer the
chosen mode: measured against APPROXIMATE, EXACT converges in ~44 solver evaluations vs ~230 and
narrows the joint-vs-staged DF drifts substantially (see the bar justifications and the
over/under-determined verdict). The solver is driven by the joint `Function_` and the block-diagonal
weights, with `tol` per residual and `controls` for iteration caps. Convergence is the existing
componentwise scaled-residual test.

### Over- or under-determined verdict (re-examination of B2 under (a))

For the example's 24-instrument, two-9-knot-curve design under PWL (2 params/knot):

- **Free parameters:** $2 \cdot 9 + 2 \cdot 9 = 36$.
- **Instruments (equations):** $12 + 12 = 24$.
- **Ratio:** $36 / 24 = 1.5$ -> the joint system is **underdetermined** (more free parameters than
  instruments), exactly as the single-curve PWL path is (`yield_curve.md:147-164`).

This is the same verdict as the (b) critique's B2(a) reached per-curve (each 12-on-9 PWL stage is
underdetermined at $18/12 = 1.5$); stacking two underdetermined stages into one system keeps the
ratio at 1.5. Consequence: the joint solve uses the smoothing weight matrix to select the unique
well-behaved member of the solution manifold.

**Solve mode: EXACT (default).** Because the system is underdetermined (36 parameters, 24
residuals), an exact solution exists, and `Underdetermined::Find` (EXACT) finds one: measured on
this example, EXACT drives every joint residual to ~$1.3 \times 10^{-9}$ in rate (~$0.00001$ bp -
still PASS, ~7 orders under the `1e-7` BAR-A gate) in **44 solver evaluations**, vs **230**
evaluations for the prior APPROXIMATE default. The earlier APPROXIMATE-era assumption that EXACT
"cannot drive residuals to zero once the tridiagonal smoother competes" is wrong and is retracted:
the smoother selects *which* member of the exact-fit manifold the solver lands on; it does not
prevent the solver from reaching that manifold. (The (b) critique's false "12-on-9 overdetermined"
justification is dropped entirely; the real story is underdetermined-plus-smoothing *member
selection*, not a residual floor.) The prior default was APPROXIMATE; switching to EXACT is strictly
better here - same PASS on BAR-A, ~5x fewer evaluations, and substantially tighter joint-vs-staged
agreement (see BAR-B and BAR-C).

### Required capability (behavioral)

The capability MUST:

- Accept a specification of the curves to calibrate jointly: per-curve `instruments_`, `knotDates_`,
  `parameterization_`, `targetCollateral_` / `targetTenor_` / `calibrateDiscountCurve_`, plus shared
  `today_`, `ccy_`, `liborBasis_`, and solver options (`solveMode_`, `fitTolerance_`,
  `smoothingWeight_`, `tolerance_`, `maxEvaluations_`, `maxRestarts_`).
- Accept the discount-curve routing: which curve supplies discounting for the IBOR stage. In the
  example this is "the OIS curve supplies OIS collateral discounting." This is the **one residual
  coupling** the joint system has under (a) (see B1 re-examination below); it is NOT a base-curve
  layering, it is forecast-instrument discounting routed to the joint OIS curve.
- Return, per curve, a calibrated `DiscountCurve_` handle plus `CurveCalibrationDiagnostics_`
  (market/model rates, residuals, `maxAbsResidual_`, `rmsResidual_`, `usedApproximateFit_`).
- Return the joint solve's diagnostics at a coarse level (joint `maxAbsResidual_`,
  `rmsResidual_`, whether the solver converged within `maxEvaluations_` / `maxRestarts_`). If the
  solver reports non-convergence, the capability throws with a message naming the failing solve and
  the residual norm (mirroring how `Underdetermined::Find` surfaces non-convergence).

### API placeholder (to be filled by `dal-api-designer`)

The example calls the joint capability as:

```cpp
// PLACEHOLDER - exact signature, struct names, and include path are dal-api-designer's decision.
// The example depends only on the behavioral contract above.
const JointMultiCurveCalibrationResult_ result = CalibrateJointMultiCurve(jointSpec);
```

Where `jointSpec` describes the curves per the "Required capability" bullets, and `result` exposes
`discountCurves_`, `forwardCurves_`, per-curve `diagnostics_`, and a joint convergence flag. The
example's `main()` body (see "Algorithm and Flow") uses these fields by name; the API step must
supply them (under whatever concrete names it chooses). **This is the only place the spec presumes a
specific shape; everything else is behavioral.**

## The Example

### Location and files

`dal-cpp/examples/joint_multi_curve_calibration/` with exactly two files:
`joint_multi_curve_calibration.cpp` and `CMakeLists.txt`, mirroring the `yield_curve_jacobian/`
layout. Registered in `dal-cpp/examples/CMakeLists.txt` (one `add_subdirectory` line, alphabetically
near `interpolate_curve`).

### Instrument set (24 instruments total, satisfies >= 20)

Self-consistent quotes derived from a synthetic flat quote market: OIS flat at 1.0%, 3M IBOR flat at
3.0% with the OIS curve as its base (so the prototype-repricing market already has the staged-style
discounting baked in - the standard `curve_calibration.cpp` construction, `lines 271-277`). Pillars
are relative to `today_ = Date_(2024, 1, 15)` (matches `curve_calibration.cpp:450`).

- **OIS discount-curve instruments (12):** 6 OIS deposits (1M, 2M, 3M, 6M, 9M, 12M) + 6 OIS swaps
  (`OISSwap_` at 2Y, 3Y, 4Y, 5Y, 7Y, 10Y).
- **3M IBOR forecast-curve instruments (12):** 6 FRAs (1x4, 2x5, 3x6, 6x9, 9x12, 12x15) + 6 vanilla
  swaps (`Swap_` at 2Y, 3Y, 4Y, 5Y, 7Y, 10Y; 3M float leg vs annual fixed leg).

This breakdown is the prior (b) spec's design; it carries over unchanged because it already meets
the >= 20 requirement with headroom and mirrors `curve_calibration.cpp`'s inventory shape.

### Shared knot grid (9 knots per curve)

`{1M, 3M, 6M, 12M, 24M, 36M, 60M, 84M, 120M}` relative to `today_` - identical to
`curve_calibration.cpp:343-349`. Both curves share the grid. With PWL (2 params/knot) each curve has
18 free parameters; the joint vector has 36 (see the over/under-determined verdict above).

Short-deposit pillars (1M, 2M, 3M) must satisfy `span.second > span.first >= today_` and land at or
before knot[0]=1M; the flat-forward extrapolation from the anchor handles any sub-first-knot
maturity (carrying over the (b) critique's B2(b) note).

### Convention choices

- `today_ = Date_(2024, 1, 15)`; currency USD; `liborBasis_ = DayBasis_("ACT_360")` (3M Libor) set
  on the shared spec, so both paths use the same basis. NOTE: `ApplyStageDefaults`
  (`calibration.cpp:110`) overwrites per-stage `liborBasis_` with the spec-level value on the staged
  path; the joint path uses the spec-level value directly. Both end up identical.
- All holidays `Holidays::None()` and conventions `BizDayConvention_("Unadjusted")` (calendar-stable,
  mirrors `yield_curve_jacobian`, not the calendar-heavy `euribor3m_curve`).
- OIS overnight index from `Ccy::Conventions::OisIndex()(Ccy_("USD"))` with
  `useProjectionCurve_ = false`, `fixingHolidays_ = accrualHolidays_ = Holidays::None()`
  (`curve_calibration.cpp:255-257`).
- 3M Libor index from `Ccy::Conventions::LiborIndex()(Ccy_("USD"))` with
  `forecastTenor_ = PeriodLength_("3M")` and the same holiday nullification.

### Joint calibration (the example's primary path)

The example builds a `jointSpec` (placeholder name, see API placeholder) carrying both curves'
instruments, knots, conventions, and the routing "OIS curve discounts the 3M stage." It calls the
joint capability, which runs ONE `Underdetermined::Find` over the 36-parameter / 24-residual system.
Both curves are calibrated simultaneously; the OIS knots and the 3M knots co-determine the residuals.

### Staged validation path

The example assembles the same two stages into a `MultiCurveCalibrationSpec_` and calls
`CalibrateMultiCurve`. This is the reference: stage 1 (OIS) calibrates the discount curve, stage 2
(3M) is calibrated with stage 1's OIS curve injected as `baseCurve_` by `ApplyStageDefaults`
(`calibration.cpp:113-117`).

### Comparison table

The example prints:

- A **per-curve residual summary** for both paths (joint and staged), per curve (OIS, 3M): max abs
  residual and RMS residual in bp.
- A **per-pillar OIS discount-factor table**: pillars `{1Y, 2Y, 3Y, 5Y, 7Y, 10Y}`, columns
  `[Date | DF_joint | DF_staged | |diff|]`, reading DFs off the OIS `DiscountCurve_` from each path.
- A **per-pillar 3M forward-curve table**: same pillars and columns, reading DFs off the 3M forward
  `DiscountCurve_` from each path.
- A **timing line**: total ms for the joint solve vs the staged solve (informational; no bar).

### Algorithm and flow

`main()`:

1. `RegisterAll_::Init()`. Set evaluation date to `today_` via `XGLOBAL::SetEvaluationDate` (mirrors
   `xccy_curve_calibration.cpp:170`).
2. Build the conventions (OIS index, 3M Libor index, fixed leg, float leg, OIS leg) and the
   synthetic flat quote market (`marketCurve` block at OIS=1.0%, 3M=3.0% over OIS) - mirrors
   `curve_calibration.cpp:271-277`.
3. Construct the 12 OIS and 12 IBOR prototype instruments with zero quotes; reprice each against
   `marketCurve` to obtain self-consistent market quotes (`QuotedInstrument`,
   `curve_calibration.cpp:46-87`).
4. Build the two per-curve stage specs (`CurveCalibrationSpec_`) exactly as in
   `curve_calibration.cpp:337-403` (same knots, EXACT solve, `fitTolerance_ = 1e-8`).
5. **Joint path**: assemble `jointSpec` from the two stage specs and call the joint capability
   (placeholder: `CalibrateJointMultiCurve(jointSpec)`). Capture `result_joint` with
   `discountCurves_`, `forwardCurves_`, per-curve `diagnostics_`, and the joint convergence flag.
6. **Staged path**: assemble `MultiCurveCalibrationSpec_ multi = {name, ccy, {oisStage,
   liborStage}, liborBasis}` and call `CalibrateMultiCurve(multi)` -> `result_staged`.
7. Print the banner, the instrument inventory line (`24 instruments (12 OIS + 12 IBOR-3M)`), and
   both paths' per-curve residual summaries.
8. Print the OIS and 3M per-pillar DF comparison tables.
9. Print the timing line.
10. Run the self-check (see below). Print `Verdict: PASS` on success; `THROW` with a descriptive
    message (naming the failing curve, pillar, and value) on failure.
11. `return 0;`.

### Comparison-table schema

Both tables (OIS and 3M) share this shape, right-aligned, `std::fixed`, DFs to 10 decimals, abs-diff
in scientific to 4 decimals:

```
======================================================================

  OIS discount curve  (joint vs staged)
----------------------------------------------------------------------
Pillar       DF_joint        DF_staged          |diff|
----------------------------------------------------------------------
1Y       0.9900495025    0.9900495025      0.0000e+00
2Y       0.9801986733    0.9801986733      1.1e-11
...
----------------------------------------------------------------------
  max |diff| ~ 3.7e-8    RMS |diff| ~ 1.8e-8     (EXACT: cross-curve coupling resolved precisely)
```

## Self-check and tolerance

Three bars. **BAR-A is the sole pass/fail THROW gate**; failure throws via a `THROW_REQUIRE`-style
macro (re-defined file-locally in the example, mirroring `yield_curve_jacobian.cpp:35-39`; that
macro is NOT in a shared header). **BAR-B and BAR-C are informational**: they print measured
joint-vs-staged DF drift for teaching and bug-detection, but they do NOT throw. (Earlier drafts of
this spec gated all three; the implemented example promotes only BAR-A to a THROW gate and reports
BAR-B/BAR-C as reference-constant measurements - see commit `6305bf5`.)

### BAR-A (instrument fit, PASS gate, both paths, both curves)

For every per-curve diagnostic in `{result_joint.diagnostics[OIS],
result_joint.diagnostics[3M], result_staged.diagnostics_[0], result_staged.diagnostics_[1]}`,
require `maxAbsResidual_ <= 10 * fitTolerance_` (= `1e-7`). Under EXACT both paths drive every
residual to ~$1.3 \times 10^{-9}$ in rate (~$0.00001$ bp), ~7 orders under the gate. Both paths
must reprice their instruments. This is the precondition for any meaningful comparison: if either
path did not converge, the comparison is meaningless. **This is the sole THROW gate** - it verifies
convergence; it does not by itself verify correctness of the cross-curve coupling (BAR-B and BAR-C
carry that signal informationally).

Additionally, every calibrated discount factor at the six pillars and at the knots must be finite
and in `(0, 1]` (loose sanity: monotonic-positive discounting).

### BAR-B (joint-vs-staged OIS-curve agreement, informational)

Across the six OIS pillars `{1Y, 2Y, 3Y, 5Y, 7Y, 10Y}`, the measured joint-vs-staged OIS DF drift
under EXACT is:

| Metric                      | APPROXIMATE (prior default) | EXACT (current default) |
|-----------------------------|-----------------------------|-------------------------|
| max \|diff\|                | $7.4343 \times 10^{-6}$     | $3.6843 \times 10^{-8}$ |
| RMS \|diff\|                | -                           | $1.7807 \times 10^{-8}$ |

The informational reference constant is `BAR_B_REFERENCE = 1e-7` (a round-up of the measured
$3.68 \times 10^{-8}$, ~$2.7\times$ margin). BAR-B is NOT a pass/fail gate; it is a printed
measurement.

**Justification (why EXACT narrows BAR-B ~200x):** both paths fit the SAME 12 OIS instruments on
the SAME 9 PWL knots with the SAME smoothing weight, tolerance, and initial guess, and both run
EXACT (`Underdetermined::Find`). The remaining drift is a *fit-quality* effect, not a round-off
effect: the joint OIS slice is co-determined with the 3M slice because the joint Jacobian's OIS
columns carry entries from the 3M residual rows (IBOR annuities discount through OIS). Under the
prior APPROXIMATE default, that cross-curve coupling was left slightly under-fit by APPROXIMATE's
regularization, producing the $7.43 \times 10^{-6}$ floor. EXACT solves the joint system exactly,
so the coupling is resolved precisely and the drift collapses to $3.68 \times 10^{-8}$ (short
pillars agree to ~$1 \times 10^{-11}$; the few-e-8 deviation concentrates at the long knots). If
this drift were to jump back to the APPROXIMATE scale (e-6) or above, that would signal either a
mis-routed OIS instrument or a joint smoothing block that diverges from the staged smoothing
matrix - both real bugs - so the measurement stays predictive even though it is not gated.

### BAR-C (joint-vs-staged 3M-curve agreement, informational)

Across the same six pillars, reading DFs off the 3M forward curve, the measured joint-vs-staged 3M
DF drift under EXACT is:

| Metric                      | APPROXIMATE (prior default) | EXACT, baseless default | EXACT, base-layered (example) |
|-----------------------------|-----------------------------|-------------------------|--------------------------------|
| max \|diff\|                | $1.1539 \times 10^{-3}$     | $3.3321 \times 10^{-4}$ | $2.39 \times 10^{-5}$          |
| RMS \|diff\|                | -                           | $1.3630 \times 10^{-4}$ | $9.77 \times 10^{-6}$          |

The example uses the **base-layered** representation (`baseLayeredOverDiscount_ = true`), so the
informational reference constant is `BAR_C_REFERENCE = 5e-5` (a round-up of the measured
$2.39 \times 10^{-5}$, ~$2\times$ margin). BAR-C is NOT a pass/fail gate; it is a printed
measurement. (The baseless table column is retained for reference; the prior `5e-4` reference applied
to that representation.)

**Justification (BAR-C is representation drift, NOT fit quality - and cannot be driven to zero by
switching solve mode):** this is the bar that distinguishes (a) from (b). Under (b), the 3M curves
differed by ~`P_OIS(T)` (several percent) because the staged path layered the OIS curve as a
multiplicative base and the simultaneous path did not. Under (a), BOTH paths discount the 3M
instruments off the OIS curve and BOTH fit the same 12 IBOR fixings to ~$1 \times 10^{-9}$ in rate
under EXACT. The remaining ~$3.3 \times 10^{-4}$ drift is therefore NOT a fit-quality issue - it is
a *which-manifold-member* question:

- The **staged** path: `ApplyStageDefaults` sets `baseCurve_ = OIS` for the 3M stage, so the staged
  3M forward curve is a `DiscountPWLF_` with `base_ = OIS` whose smoothing target is the OIS
  *spread*; its discount factor factorizes as `P_3M_staged(T) = P_spread(T) * P_OIS(T)`
  (`yield_curve.md:117`).
- The **joint** path: the 3M forward curve is a raw piecewise-linear-forward curve with NO `base_`
  handle, smoothing the *absolute* 3M forward; its discount factor is the raw 3M curve's
  `P_3M_joint(T)`.

Because $f_{\text{absolute}} = f_{\text{spread}} + f_{\text{OIS}}$ and the OIS curve has curvature,
the two smoothing targets pick *different valid members* of the exact-fit PWL manifold. Both members
fit the IBOR fixings to ~$1 \times 10^{-9}$; their `curve(today, T)` outputs agree to a few e-6 in
the 2Y-7Y core and drift up to ~$3.3 \times 10^{-4}$ at the short end (the 1Y pillar at $3.3 \times
10^{-4}$ dominates; longer pillars sit at ~$5 \times 10^{-6}$), where the absolute level is least
constrained. **This is legitimate representation drift, not a bug - which is why BAR-C stays
informational and cannot be tightened by switching solve mode:** EXACT narrowed the short-end drift
vs APPROXIMATE ($1.15 \times 10^{-3} \to 3.3 \times 10^{-4}$, ~$3.5\times$) by resolving the fit
precisely, but it cannot eliminate the drift, because the mismatch is in *which* member of the
manifold each parameterization picks, not in fit quality. The only way to close BAR-C would be to
switch the joint 3M curve to the same `DiscountPWLF_`-with-OIS-base parameterization as the staged
path - which the approved design explicitly does not do (Non-Goals: no new parameterization).

**The measurement stays predictive despite being informational:** if the joint solve mis-routes the
3M discounting (e.g. forgets to use the OIS curve), the 3M curves will differ by ~`P_OIS(T)`
(percent-level) - orders of magnitude above `5e-4` - and the printed drift will flag it immediately.

If in practice the joint-vs-staged 3M difference drifts above the measured ~$3.3 \times 10^{-4}$
(toward the `5e-4` reference), the implementer should report it; the reference may be widened with a
documented measurement, but must NOT be widened into the percent range (that would erase the
bug-detection power).

### Structural caveat: the joint 3M curve is NOT the staged 3M curve (B-new-2)

**Update (2026-06-20): B-new-2 is FIXED for the opt-in base-layered path.** When
`baseLayeredOverDiscount_ = true` (the example's setting), the stored joint 3M curve IS a
`DiscountPWLF_` with `base_ = OIS`, structurally identical to the staged 3M curve, and an OIS bump
propagates through the `base_` handle. The joint and staged 3M curves ARE then interchangeable as
risk objects. The caveat below applies to the **baseless default** (`baseLayeredOverDiscount_ = false`),
which remains supported.

The joint and staged 3M curves AGREE numerically (BAR-C), but in the baseless default they are
**structurally different curve objects** - they are NOT
interchangeable for anything beyond `curve(today, T)` reads. The first risk consumer who does
bump-and-reprice through the standalone joint 3M `DiscountCurve_` will walk into a silent
zero-sensitivity result, and the spec must flag this so it is not discovered in production.

- **Staged 3M curve** is a `DiscountPWLF_` with `base_ = OIS` handle
  (`dal-cpp/dal/curve/ycimp.cpp:59-65`, assigned at
  `dal-cpp/dal/curve/calibration.cpp:212` via `ApplyStageDefaults`). Its discount factor
  factorizes as $P^{\text{3M}}_{\text{staged}}(T) = P_{\text{spread}}(T) \cdot P_{\text{OIS}}(T)$
  (`docs/methodology/yield_curve.md:117`). Bumping the OIS curve flows through `base_` into the
  staged 3M curve automatically (`dal-cpp/dal/curve/yccomponent.hpp:24-49`,
  `CurveWithBase_<DiscountCurve_>`).
- **Joint 3M curve** is a raw piecewise-linear-forward curve with NO `base_` handle - the spec's
  "no base concept" wording (the B1 re-examination above: the joint path has no base layering at
  all). Its discount factor is $P^{\text{3M}}_{\text{joint}}(T) = \exp(-\int f_{\text{pwl}}/365)$,
  with the OIS discounting absorbed into the knot values themselves (forced by the IBOR residual
  rows reading `yc.Forward(3M, OIS)`, per the routing invariant above). **Bumping the OIS slice of
  the joint solution does NOT move the joint 3M curve** - it has no `base_` handle and no path
  from OIS to its stored forwards.

**Why BAR-C still passes despite this mismatch:** both curves are calibrated against identical
IBOR fixings (the same 12 IBOR instruments on the same 9 knots), and those fixings read
`yc.Forward(3M, OIS)`, so the data forces the joint 3M PWL forwards to absorb the combined factor
that the staged path splits into `spread * OIS`. The `curve(today, T)` outputs therefore agree -
but only as scalar outputs at read time. The stored representations, the smoothing targets
(`f_pwl` vs `f_spread`), and the OIS-sensitivity behavior are all different.

**Scope / consumer contract:** This is consistent with the Non-Goals (no AAD analytic Jacobian,
no risk work in the first cut). Consumers that need OIS-bump sensitivity through the 3M curve
must re-price through the assembled `CurveBlock_` (which carries both curves and routes
correctly), NOT through the standalone joint 3M `DiscountCurve_`. Documenting this representation
mismatch in the spec is THIS spec's job; the corresponding API-note row (consumer contract for the
stored joint forward curves) is `dal-api-designer`'s job.

### What is deliberately NOT asserted

- **Exact identity of the joint and staged 3M curves to machine precision.** They are not guaranteed
  to be byte-for-byte identical: the joint 3M curve is a raw PWL curve smoothing the absolute
  forward, while the staged 3M curve is a `DiscountPWLF_` with OIS base smoothing the spread - two
  different parameterizations that pick different (but equally valid) members of the exact-fit
  manifold. BAR-C's measured ~$3.3 \times 10^{-4}$ captures the legitimate representation drift;
  asserting tighter would be wrong, and no solve-mode switch can eliminate it (see BAR-C
  justification).
- **Timing.** Informational only.

## Re-examination of the (b) Critique's findings under (a)

The prior critique (`.claude/critiques/multi-curve-simultaneous-example.md`) raised B1/B2/B3 under
interpretation (b). Each is re-examined under (a) here; the spec encodes the conclusion.

### B1 (inject OIS as discount curve for a "3M stage") - ADAPTED, not moot

Under (b), B1 was blocking because the "simultaneous 3M stage" called `CalibrateYieldCurve` with no
discount curves loaded, which throws at the first residual (`CurveBlock_` ctor REQUIREs a non-empty
discount map, `curveblock.cpp:35-49`). Under (a), there are **no sequential per-curve stages** - the
notion of "injecting the OIS result into a 3M stage" does not arise as a staging step.

**But the underlying coupling does not disappear.** The IBOR instruments (FRAs, vanilla swaps) must
still be discounted, and post-2008 routing discounts them off the OIS curve, not off the 3M curve
itself. So the joint residual function MUST route IBOR-instrument discounting to the OIS slice of the
joint parameter vector. This is a property of the joint residual function's construction (it builds
both curves from `x`, assembles a `CurveBlock_`, and prices through it - see "Joint residual
system"), not a base-curve injection step. The distinction that mattered under (b) ("inject OIS as
base vs inject OIS as discount") collapses under (a): there is one routing decision (OIS discounts
IBOR), expressed once in the joint residual function, with no staging and no `baseCurve_` field
involved.

**Conclusion: B1 is ADAPTED.** The coupling it pointed at (OIS must discount IBOR) is real and is
expressed in the joint residual function's routing. The staging machinery (`ApplyStageDefaults`,
`baseCurve_`, `LoadDiscountCurves`) is not used by the joint capability. The (b) critique's S1
wording fix ("discount curve but NOT base curve") is absorbed: under (a) the joint path has no base
concept at all.

### B2 (free-parameter count) - RECOMPUTED for the joint system

See "Over- or under-determined verdict" above. Joint free parameters = $2 \cdot 9 + 2 \cdot 9 = 36$;
instruments = $24$; ratio $1.5$ -> **underdetermined**. EXACT (`Underdetermined::Find`) is chosen
because an underdetermined PWL system with 24 self-consistent quotes has an exact solution, and
`Find` reaches it - measured ~$1.3 \times 10^{-9}$ in rate on every residual, in 44 evaluations vs
APPROXIMATE's 230. The earlier "APPROXIMATE is chosen because the underdetermined-plus-smoothing
system leaves a residual the EXACT solver cannot drive to zero" rationale (which matched
`curve_calibration.cpp:405-412`'s then-stated reason) is retracted: the tridiagonal smoother selects
*which* member of the exact-fit manifold the solver lands on; it does not impose a residual floor.
The (b) critique's B2(b) short-deposit note (`span.second > span.first >= today_`, land at or before
knot[0]) carries over verbatim.

### B3 (self-check) - ADAPTED to validate joint-vs-staged

Under (b), B3 proposed a base-layering ratio check (`DF_3M_staged / DF_3M_simul == P_OIS factor`)
because the two 3M curves differed by design. Under (a) the two 3M curves AGREE closely (both
discount off OIS, both EXACT), so the base-layering ratio check is replaced by BAR-C (informational
joint-vs-staged 3M DF drift, measured ~$3.3 \times 10^{-4}$ under EXACT, reference `5e-4`). BAR-B
(informational joint-vs-staged OIS DF drift, measured ~$3.68 \times 10^{-8}$ under EXACT, reference
`1e-7`) replaces the (b) OIS-identity sanity bar. BAR-A (instrument fit, both paths, `1e-7`) is the
SOLE pass/fail gate and carries over unchanged as the convergence precondition. BAR-B and BAR-C are
informational measurements, not gates; each stays predictive in the sense that a gross regression
(mis-routed OIS or 3M discounting) would push the printed drift into percent-level territory and be
flagged immediately.

The (b) critique's honesty about residual floors (B3(a): the bar floor is a real measurement, not
round-off) carries over and is now grounded in measured EXACT numbers, not the prior APPROXIMATE
residual-smoothing scale: BAR-B's `1e-7` references the measured $3.68 \times 10^{-8}$, BAR-C's
`5e-4` references the measured $3.33 \times 10^{-4}$. If a drift measurement changes materially (a
different compiler, AAD backend, or knot grid), the implementer re-measures before widening the
reference, and reports it; arbitrary widening is not permitted, and no reference may be widened into
the percent range (that would erase the bug-detection power).

## Non-Functional Requirements

- **Build time** - the example is a single translation unit plus the new library TU(s) for the joint
  capability. No target.
- **Runtime** - one joint `Underdetermined::Find` over a 36-parameter / 24-residual system (bumped
  Jacobian, no AAD), plus one staged `CalibrateMultiCurve`. Sub-second to a few seconds on any
  modern machine depending on Jacobian bump count. Informational timing printed; no bar.
- **Differentiability** - none required by the example or the capability's first cut. The joint
  residual function returns `nullptr` from `Gradient` (bumped path), matching default
  `CurveJacobianMode_::Value_::BUMPED`. AAD analytic-Jacobian extension for the joint system is
  out of scope (Non-Goals).
- **Compatibility** - the joint capability is a NEW public API (new header, new entry point). It
  must NOT change any existing signature in `calibration.hpp` or `curveblock.hpp`. The existing
  `CalibrateYieldCurve` and `CalibrateMultiCurve` paths must remain byte-for-byte unchanged (the
  staged validation path depends on this). Full `bin/dal_cpp_tests` must stay green.
- **Backend neutrality** - the joint capability and the example must build and run under every AAD
  backend preset (none, Adept, XAD, CoDiPack). Because neither engages AAD, this is automatic; the
  example's CMake carries the backend branches only for shape consistency with
  `yield_curve_jacobian/CMakeLists.txt` (they are dead code in this example).

## Inputs and Outputs

No external inputs (no file reads, no CLI args). Outputs are stdout only.

| Name                          | Type                           | Units             | Range / Constraints                              |
|-------------------------------|--------------------------------|-------------------|--------------------------------------------------|
| `today_`                      | `Date_`                        | calendar date     | Fixed `2024-01-15`                               |
| OIS deposit quotes            | `double` (per instrument)      | decimal rate      | Repriced off 1.0% flat OIS market                |
| OIS swap quotes               | `double`                       | decimal par rate  | Repriced off 1.0% flat OIS market                |
| 3M FRA quotes                 | `double`                       | decimal forward   | Repriced off 3.0% flat 3M (2.0% over OIS)        |
| 3M swap quotes                | `double`                       | decimal par rate  | Repriced off 3.0% flat 3M (2.0% over OIS)        |
| `fitTolerance_`               | `double`                       | decimal rate      | `1e-8` (both curves, both paths)                 |
| `smoothingWeight_`            | `double`                       | dimensionless     | `1.0` (both curves, both paths)                  |
| BAR-A tolerance (PASS gate)   | `double`                       | decimal rate      | `1e-7` (= 10 * `fitTolerance_`); sole THROW gate |
| BAR-B reference (OIS DF drift)| `double`                       | discount factor   | `1e-7` (informational; measured EXACT 3.68e-8)   |
| BAR-C reference (3M DF drift) | `double`                       | discount factor   | `5e-4` (informational; measured EXACT 3.33e-4)   |
| `solveMode_`                  | `CurveSolveMode_::Value_`      | enum              | `EXACT` (library default; both paths)            |
| stdout comparison tables      | text                           | -                 | Two tables (OIS, 3M), 6 pillars each             |

## File and CMake Layout

```
dal-cpp/examples/joint_multi_curve_calibration/
    CMakeLists.txt
    joint_multi_curve_calibration.cpp
```

Plus new library TU(s) under `dal-cpp/dal/curve/` for the joint capability (exact files are
`dal-api-designer`'s call; likely a new `jointcalibration.hpp` / `.cpp` pair, or an extension of
`calibration.hpp`). The example includes the new public header.

### `CMakeLists.txt` (verbatim shape, mirror `yield_curve_jacobian/CMakeLists.txt`)

```cmake
file(GLOB_RECURSE JOINT_MULTI_CURVE_CALIBRATION_FILES "*.hpp" "*.cpp")
add_executable(joint_multi_curve_calibration ${JOINT_MULTI_CURVE_CALIBRATION_FILES})

target_link_libraries(joint_multi_curve_calibration dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(joint_multi_curve_calibration XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(joint_multi_curve_calibration CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(joint_multi_curve_calibration adept)
endif()

if(MSVC)
else()
    target_link_libraries(joint_multi_curve_calibration pthread)
endif()

install(TARGETS joint_multi_curve_calibration
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
```

### `dal-cpp/examples/CMakeLists.txt` (one-line addition)

Add `add_subdirectory(joint_multi_curve_calibration)` in alphabetical position (after
`interpolate_curve`, before `xccy_curve_calibration`).

## Acceptance Criteria

### Capability (library)

- [ ] A new public API entry point exists (header + implementation) that calibrates a multi-curve
  set jointly via a single `Underdetermined::Find` call over the concatenated free-parameter vector,
  per the "Required capability" section. Exact signature is `dal-api-designer`'s output.
- [ ] The joint residual function builds ALL curves from `x`, assembles a `CurveBlock_`, and returns
  the stacked residuals of every curve's instruments; the OIS slice discounts the IBOR instruments
  (routing validated by BAR-C passing).
- [ ] The smoothing weight matrix is block-diagonal (one tridiagonal block per curve, zero
  off-block).
- [ ] No existing public signature in `calibration.hpp` or `curveblock.hpp` is changed; existing
  `CalibrateYieldCurve` and `CalibrateMultiCurve` paths are byte-for-byte unchanged.
- [ ] On solver non-convergence, the capability throws with a message naming the failing solve and
  the residual norm.
- [ ] A new unit test under `dal-cpp/tests/curve/` (e.g. `test_joint_calibration.cpp`) calibrates a
  small joint system and asserts (i) both curves' `maxAbsResidual_` within tolerance, and (ii) the
  joint OIS curve agrees with a single-curve `CalibrateYieldCurve` on the same OIS instruments to a
  tight tolerance. Test style follows `.claude/rules/unit-test-style.md`.

### Example

- [ ] New directory `dal-cpp/examples/joint_multi_curve_calibration/` exists with exactly two files
  (`CMakeLists.txt`, `joint_multi_curve_calibration.cpp`).
- [ ] `dal-cpp/examples/CMakeLists.txt` registers the new subdirectory and the build produces
  `bin/joint_multi_curve_calibration`.
- [ ] `bin/joint_multi_curve_calibration` runs to completion and prints `Verdict: PASS` (exit code
  0) under the default Linux build (`bash ./build_linux.sh`).
- [ ] The instrument inventory line prints `24 instruments (12 OIS + 12 IBOR-3M)`, satisfying the
  user's "no less than 20" requirement.
- [ ] Both paths' residual summaries show `max abs residual <= 1e-7` (BAR-A, the sole THROW gate),
  printed in the residual table.
- [ ] The OIS comparison table prints the joint-vs-staged drift with the measured max ~`3.7e-8`
  (BAR-B, informational; reference `1e-7`), confirming both paths route OIS instruments off the OIS
  curve under EXACT.
- [ ] The 3M comparison table prints the joint-vs-staged drift with the measured max ~`3.3e-4`
  (BAR-C, informational; reference `5e-4`), confirming both paths discount the 3M instruments off
  the OIS curve (the joint-vs-staged agreement is the teaching payload under (a)); the residual
  representation drift (raw PWL forward vs `DiscountPWLF_` + OIS-base spread) is expected and is
  NOT a bug.
- [ ] If the joint solver reports non-convergence, the program throws with a message naming the
  failing solve (no silent PASS on a non-converged run).
- [ ] The file header matches the three-line convention; the code passes
  `.claude/rules/code-style.md` (4-space indent, 150-col limit, `using namespace Dal;`, PascalCase
  functions, anonymous namespace for helpers, `constexpr` tolerance bars).
- [ ] Full `bin/dal_cpp_tests` and `ctest --output-on-failure` remain green.

### Documentation

- [ ] `docs/methodology/yield_curve.md` (or a new `docs/methodology/joint_multi_curve.md` linked
  from it) gains a section describing the joint multi-curve solve, its block-diagonal smoothing, and
  the over/under-determined verdict. Defer to `dal-doc-writer`.

## Open Questions

1. **(API shape, for `dal-api-designer`)** Should the joint spec reuse `MultiCurveCalibrationSpec_`
   (adding a flag like `joint_ = true`) or introduce a distinct `JointMultiCurveCalibrationSpec_`?
   The spec is agnostic; the API step decides. The example only needs the behavioral contract.
2. **(BAR-C reference)** `5e-4` is the informational reference for joint-vs-staged 3M DF drift,
   rounded up from the measured EXACT max `3.33e-4` (RMS `1.36e-4`). The drift is representation
   drift (raw PWL absolute forward vs `DiscountPWLF_` OIS-base spread smoothing), NOT fit quality,
   so it cannot be eliminated by switching solve mode. If a future measurement (different
   compiler/backend/knot grid) shows the actual drift materially different, the implementer
   re-measures and may widen the reference with a documented measurement. The reference must stay
   well below percent-level to preserve bug-detection power.
3. **(Smoothing matrix shape)** Block-diagonal (one tridiagonal block per curve) is this spec's
   choice. An alternative is a single tridiagonal operator over the concatenated knots, which would
  couple smoothing across curves at the boundary. Block-diagonal is simpler and matches the
   per-curve semantics; flagged in case the API step prefers the concatenated alternative.
4. **(Diagnostics granularity)** The spec asks for per-curve `CurveCalibrationDiagnostics_` plus a
   coarse joint convergence flag. A richer joint diagnostics (joint Jacobian, joint
   `effJacobianInverse_`) is out of scope for the first cut but may be wanted for risk work; flag for
   the API step.
5. **(Author/date in the example file header)** Default: `dal-spec-writer` / 2026-06-20. The user
   may want their own name.

## Hand-Off

This spec defines a NEW public API whose exact signature is intentionally left open. The next agent
is **`dal-api-designer`**, which must:

1. Choose the concrete C++ signature, struct names, and include path for the joint capability,
   satisfying the "Required capability" and "API placeholder" sections.
2. Decide the four Open Questions that fall to the API step (1, 3, 4; 2 and 5 are for the
   implementer/user).
3. Produce an API note that the implementer and the example both consume.

After the API note is signed off, `dal-implementer` implements the capability + example + unit test,
and `dal-doc-writer` updates the methodology doc. The example mirrors
`dal-cpp/examples/curve_calibration/curve_calibration.cpp` for instrument construction and
`dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp` for file shape, banner/table
helpers, and the `RegisterAll_::Init()` entry point.

# Joint Multi-Curve AAD Analytic Jacobian (Phase B) - Specification

> Status: **REVISED (third pass) — 2026-06-20.** This pass DROPS the second-pass
> CP4 / critique-B5 scope reduction (reject `OISSwap_`, switch the example to
> vanilla `Swap_`, defer `Tape::OisSwapRate_<T_>`). The lead re-verified
> `dal-cpp/dal/curve/ycinstrument.cpp`: the library has NO geometric overnight
> compounding — the AAD path (`Tape::SwapRate_<T_>::operator()`,
> `ycinstrument.cpp:347-368`) and the double path (`ForwardRate`,
> `ycinstrument.cpp:53-59`) price OIS with the IDENTICAL simple-rate arithmetic
> `(1/DF - 1) / basis`, accumulated per period as `fixing * dcf * DF`. AAD and
> bumped evaluate the SAME function on OIS rows, so the AAD-vs-bumped oracle (AC1)
> PASSES for OIS. The OIS overnight index has `useProjectionCurve_ == false`
> (`ycinstrument.cpp:44`) -> `forecast == discount == OIS`, so the inherited
> `Swap_::PrecomputeT<T_>` is BOTH routing-compatible AND gradient-correct for OIS.
> **OIS is restored to full ANALYTIC; the example's OIS swaps stay `OISSwap_`.**
> Option B (extend AAD to `PIECEWISE_LINEAR_FWD` via a new `Tape::DiscountPWLF_<T_>`
> with a templated base handle) remains LOCKED. Default `jacobianMode_` = `ANALYTIC`,
> matching single-curve. The IBOR(3M) projection slice (where `forecast != discount`)
> is priced through a NEW `Tape::JointRate_<T_>` base whose `operator()` takes a
> `const JointCurveBlock_<T_>&` (CP3) — NOT through Phase A's `Tape::Rate_<T_>` virtual
> (which is bound to `YCCtx_<T_>` and reads a single curve). The OIS-discount slice
> (`forecast == discount`) rides the inherited `Swap_::PrecomputeT<T_>`. This is a
> DESIGN-ONLY spec. No code, no headers, no test scaffolding. Implementation is the
> `dal-implementer`'s job; API shape is `dal-api-designer`'s job; algorithm selection
> is the architect's job.

## Source

- **Ask:** user request on 2026-06-20 — Phase B of the AAD-analytic-Jacobian effort.
- **LOCKED scope decision (user, 2026-06-20):** Option B — extend AAD to
  `PIECEWISE_LINEAR_FWD`. Author a new templated `Tape::DiscountPWLF_<T_>` that
  interpolates forwards on `T_` and integrates forwards to log-DF, with a TEMPLATED
  BASE HANDLE so joint forward curves are base-layered over the OIS discount curve
  and that base propagates through the tape. Default `jacobianMode_` = `ANALYTIC`.
- **Concrete target:** replace the `nullptr` return in
  `JointResidualFunction_::Gradient` (`dal-cpp/dal/curve/jointcalibration.cpp:356-357`,
  comment "Bumped Jacobian only. No AAD in the first cut") with a real AAD
  reverse-sweep Jacobian, backend-neutral across all four AAD backends
  (native / XAD / CoDiPack / Adept).
- **Branch:** `feature/multi-curve-joint-calibration` at `b5b8bbf`. Branch off it; do
  NOT merge (user's action).
- **Phase A reference (shipped):**
  `dal-cpp/dal/curve/calibration.cpp` — `YieldCurveCalibrationFunc_::AnalyticJacobian`
  (single-curve, LOG_DISCOUNT-only, all four backends). Its recording contract
  (`Clear → RegisterIndependent → NewRecording → forward eval → per-row
  {ZeroAdjoints, Adjoint=1, PropagateToStart, harvest}`) is reused verbatim by the
  joint path; its `Tape::DiscountLogDF_<T_>` is NOT (the joint path uses PWL_FWD).
- **Authoritative design docs (cite, do not re-derive):**
  - `.claude/designs/aad-analytic-jacobian-redesign.md` — executive decision doc;
    its **"Implementation realities"** section overrides the older sub-docs on three
    specifics (XAD recording order; Adept `ZeroGradientArray`; `static_cast<double>`
    portability).
  - `.claude/designs/aad-analytic-jacobian-backend-abstraction.md` — the `Dal::AAD`
    facade primitives and the single-result reverse-sweep loop.
  - `.claude/designs/aad-analytic-jacobian-naming-and-flag.md` — the
    `CurveJacobianMode_{BUMPED, ANALYTIC}` runtime flag, options-struct placement,
    NOTICE-once contract.
  - `.claude/designs/aad-analytic-jacobian-selector-api.md` — historical CP1-era
    selector; its C++ enum was dropped, revived two-value in Phase A.

## Problem Statement

Phase A delivered a backend-neutral AAD reverse-sweep Jacobian for the SINGLE-curve
calibration (`CalibrateYieldCurve`), eligible for LOG_DISCOUNT + DISCOUNT-target +
no-projection-curve + vanilla Deposit/FRA/Future/Swap calibrations. The joint
multi-curve calibration (`CalibrateJointMultiCurve`) — which solves all discount and
forward curves simultaneously from one stacked parameter vector — has no AAD path
today: `JointResidualFunction_::Gradient` returns `nullptr`, forcing the solver to
dense-bump every free parameter on every iteration. For a joint system with `N` curves
and `P` total free parameters, dense-bumping costs `P + 1` residual evaluations per
Jacobian, and the joint `F` rebuilds every declaration's curve plus a `CurveBlock_`
each time — a measured (but unquantified here) hot path. The joint system also carries
the one cross-curve coupling the single-curve path never had: IBOR float legs fix off
a forward curve and discount off a separate discount curve, so an OIS knot
perturbation flows into the 3M forward curve's discount factors (via the IBOR leg's
discounting) AND through the base handle of any base-layered forward declaration
(`f_abs = f_spread + f_ois`); bumping re-discovers both couplings numerically, slowly.
An AAD Jacobian captures both couplings exactly and in one sweep per residual row.

## Goals

- **G1.** Replace `JointResidualFunction_::Gradient`'s `nullptr` with a real AAD
  reverse-sweep Jacobian over the JOINT stacked parameter vector, returned as a
  `Underdetermined::Jacobian_*` the existing solver consumes unchanged.
- **G2.** Make the joint AAD path backend-neutral: the same source compiles and
  produces a correct Jacobian under native, XAD, CoDiPack, and Adept. Reuse the
  `Dal::AAD` facade primitives (`RegisterIndependent`, `ZeroAdjoints`, `Adjoint`,
  `PropagateToStart`, `NewRecording`, `Clear`, `Value`, `Tape()`) that Phase A
  already exercises; do NOT add new facade primitives for the joint path.
- **G3.** Author a NEW templated `Tape::DiscountPWLF_<T_>` that interpolates forwards
  on `T_` (piecewise-linear in the forward rate, 2 params/knot: `fLeft` and `fRight`)
  and integrates forwards to log-DF, and that SUPPORTS A TEMPLATED BASE HANDLE
  (`base : CurveWithBase_<DiscountCurve_<T_>>`) so joint forward curves base-layered
  over the OIS discount curve propagate the base through the tape. The forward-rate
  interpolation and forward-to-log-DF integration must be `T_`-typed end-to-end; the
  `365.0` denominator and the knot abscissae stay `double`. The `double`-typed
  `DiscountPWLF_` in the anonymous namespace at `dal-cpp/dal/curve/ycimp.cpp:56-83`
  is the reference for the arithmetic body; the templated class lives under
  `namespace Dal::Tape` alongside `Tape::DiscountLogDF_<T_>`.
- **G4.** Author a NEW templated `Tape::JointRate_<T_>` rate hierarchy (CP3, critique
  B4) so the joint residual can be evaluated entirely in the `Number_` domain through
  a `JointCurveBlock_<T_>` routing context (Gap 1), including (i) building every
  declaration's curve from its slice of `x` (PWL_FWD with 2 params/knot, every knot
  free — no anchor exclusion), (ii) assembling the multi-curve routing the IBOR legs
  need, and (iii) pricing every instrument through that routing, reading BOTH a
  forecast curve and a discount curve in the `Number_` domain. The new
  `Tape::JointRate_<T_>` base and its projection-capable subclasses are SIBLINGS of
  Phase A's `Tape::Rate_<T_>` hierarchy — Phase A's `Tape::Rate_<T_>` virtual (bound
  to `YCCtx_<T_>`), its `YCCtx_<T_>` context, and its four rate subclasses are
  UNTOUCHED (NG2).
- **G5.** Provide a joint-analogue eligibility predicate (mirroring Phase A's
  `EligibleForAnalyticJacobian`) that gates the AAD path, emits a per-condition
  `NOTICE` naming the offending input on fall-through, NEVER throws, and is evaluated
  once and cached so the `NOTICE` budget is at most once per
  `CalibrateJointMultiCurve` call.
- **G6.** Expose a `CurveJacobianMode_` knob on the joint options surface so a user
  can pick `BUMPED` vs `ANALYTIC` per call, with default `ANALYTIC` (matching the
  single-curve default) and a one-line migration note that existing callers now
  exercise the AAD path by default.
- **G7.** Verify correctness with the definitive AAD-vs-bumped Jacobian agreement
  test on a small joint system, run under each of the four backends.

## Non-Goals

- **NG1.** Multi-result / vector-mode AAD (the `EvalJacobian` per-backend fast path
  the Phase A redesign deferred under a profiling gate). Ship the single-result loop;
  the `~nRows x` speedup is a follow-up.
- **NG2.** Any change to the public SINGLE-curve `CalibrateYieldCurve` API or its
  AAD path. Phase A is shipped; this spec touches the joint path only. The single-curve
  `Tape::DiscountLogDF_<T_>` is untouched (Option B introduces a SIBLING
  `Tape::DiscountPWLF_<T_>`, not a modification).
- **NG3.** Python or Excel bindings for the joint AAD path. The single-curve path
  has none; the joint path adds none.
- **NG4.** `BasisSwap_` instruments, `STIR_`, or any instrument type outside
  `{Deposit_, FRA_, Future_, Swap_}`. `OISSwap_` IS in scope: it inherits `Swap_`
  (`ycinstrument.hpp:155`) and its overnight index has `useProjectionCurve_ == false`
  (`ycinstrument.cpp:44`), so it forecasts off the discount curve
  (`forecast == discount == OIS`) and rides the inherited `Swap_::PrecomputeT<T_>` —
  routing-compatible AND gradient-correct (the AAD and double paths share the
  identical simple-rate arithmetic; AC1 passes for OIS). `BasisSwap_` and `STIR_` are
  rejected (no Phase A templated rate) with a one-time NOTICE and bumped fallback for
  the whole joint solve. (The second-pass CP4 rejection of `OISSwap_` is RETIRED —
  see Gap 5.)
- **NG5.** A compile-time kill switch (`DAL_DISABLE_CURVE_ANALYTIC_JACOBIAN`
  equivalent). The runtime enum covers every documented use case; the kill switch is
  deferred exactly as in Phase A.
- **NG6.** Parallelization of the residual loop or the reverse sweep. Every backend
  uses a `thread_local` tape; the joint path stays single-threaded.
- **NG7.** Persisting `jacobianMode_` in any serialized form. The mode lives on a
  per-call options struct, NOT on `JointMultiCurveCalibrationSpec_` or
  `JointCurveDeclaration_`, matching Phase A's H2 resolution.
- **NG8.** A `usedAnalyticJacobian_` diagnostics flag. Asserting that the analytic
  path engaged is a separable diagnostics follow-up (Phase A Open Question 2), out of
  scope here.
- **NG9.** Extending AAD to `PIECEWISE_CONSTANT_FWD`, `ZERO_RATE`, or any
  parameterization other than `PIECEWISE_LINEAR_FWD` and `LOG_DISCOUNT`. The joint
  module's default and the joint example both use `PIECEWISE_LINEAR_FWD`; Option B
  AADs that. `LOG_DISCOUNT` remains AAD-eligible via Phase A's single-curve path only
  (it is not the joint default and is out of scope for the JOINT AAD path under
  Option B's first cut).
- **NG10.** `Tape::OisSwapRate_<T_>` — a geometrically-compounded overnight-index
  swap templated rate. NOT needed in Phase B: the library has no geometric overnight
  compounding, so `OISSwap_` rides the inherited `Swap_::PrecomputeT<T_>` (simple-rate
  arithmetic, `forecast == discount`) and is eligible for ANALYTIC. If a future
  requirement adds genuine daily overnight compounding to the curve module, a
  templated compounded rate would be authored then; until that day this is a non-goal
  that costs nothing. (The second-pass CP4 deferral of `Tape::OisSwapRate_<T_>` is
  RETIRED with CP4 — see Gap 5.)
- **NG11.** Non-default day bases. The templated `Tape::DiscountPWLF_<T_>` routes
  its forward-to-log-DF denominator through the `DAYS_PER_YEAR = 365.0` constant
  (`ycconst.cpp:16`) and the eligibility predicate (FR3 (j)) rejects any spec whose
  `liborBasis_ != ACT_365F`. Supporting `ACT/360`, `30/360`, etc. requires routing
  the denominator through a `DayBasis_` and fixing the underlying inconsistency in
  the double `DiscountPWLF_` at `ycimp.cpp:65` (out of scope under NG2). Deferred.

## The Four Gaps (Phase A → Phase B)

The four structural gaps that make this Phase B, not a copy-paste of Phase A. Each is
a framing the spec must address; the implementation approach is the architect's job,
but the spec pins WHAT must be AAD-differentiable and WHICH instrument types are in
scope.

### Gap 1 — Multi-curve: one `x`, N curves, one routing context

Phase A's `AnalyticJacobian` builds ONE `Tape::DiscountCurve_<Number_>` and prices
every instrument through `Tape::YCCtx_<Number_>(thatCurve)`. The joint residual
(`JointResidualFunction_::F`, `jointcalibration.cpp:311-354`) builds EVERY
declaration's curve from its slice of `x`, assembles them into ONE `CurveBlock_`, and
prices every instrument through that block. The whole curve-build +
`CurveBlock_`-assembly + residual-stacking must be templated on `T_`. Confirmed
against source:

- `Tape::YCCtx_<T_>` (`dal-cpp/dal/curve/ycctx.hpp:18-22`) carries exactly ONE
  `const DiscountCurve_<T_>& curve_` and has NO `Forward(...)` member.
- `class CurveBlock_` (`dal-cpp/dal/curve/curveblock.hpp:19-40`,
  `curveblock.cpp:33-83`) is NOT templated; it only ever holds
  `Handle_<DiscountCurve_>` (= `Tape::DiscountCurve_<double>`). There is NO
  `Tape::CurveBlock_<T_>` / `CurveBlockT_<T_>` anywhere in the tree.

**Spec consequence:** the joint AAD path needs a templated analogue of the
forecast-vs-discount routing that `CurveBlock_` provides in the double domain —
something that holds one `DiscountCurve_<T_>` per collateral and one per tenor and
exposes both `Discount(...)` and `Forward(...)` reads in the `Number_` domain. Whether
that is a new `Tape::CurveBlock_<T_>` class, a broadened `Tape::YCCtx_<T_>`, or a
pair of maps passed alongside the existing context is a design decision for
`dal-api-designer` / the architect. The spec pins only the requirement: every
discount-factor read and every forecast-rate read the joint residual performs in the
double path must have a `Number_`-typed counterpart on the AAD path, and the same
`(collateral, tenor)` routing keys must be honoured.

### Gap 2 — Parameterization fork: Option B LOCKED (extend AAD to PWL_FWD)

Phase A is LOG_DISCOUNT-only. Its templated curve builder
`BuildDiscountCurveT<T_>` (`calibration.cpp:239-256`) hard-`REQUIRE`s
`CurveParameterization_::Value_::LOG_DISCOUNT` and constructs
`Tape::DiscountLogDF_<T_>`. The joint `JointCurveDeclaration_` DEFAULTS to
`PIECEWISE_LINEAR_FWD` (`jointcalibration.hpp:47`), the joint example program
(`dal-cpp/examples/joint_multi_curve_calibration/`) uses `PIECEWISE_LINEAR_FWD`, and
the joint validator already supports `PIECEWISE_LINEAR_FWD` natively
(`ParamsPerKnot(PIECEWISE_LINEAR_FWD) == 2` at `jointcalibration.cpp:36-39`;
`BuildDeclarationCurve` builds it at `:74-84`). Confirmed against source:

- No `Tape::DiscountPWLF_<T_>` exists today. The only templated curve under `Tape::`
  is `DiscountLogDF_<T_>` (LOG_DISCOUNT, at `dal-cpp/dal/curve/yclogdf.hpp:25`). The
  double-typed `DiscountPWLF_` lives in an anonymous namespace at
  `dal-cpp/dal/curve/ycimp.cpp:56-83` and is only reachable via the `NewDiscountPWLF`
  factory at `:85-89`. It is NOT templated.
- The double `DiscountPWLF_::operator()` body is
  `exp(-(fwds_.IntegralTo(to) - fwds_.IntegralTo(from)) / 365.0) * (base_ ? (*base_)(from, to) : 1.0)`
  (`ycimp.cpp:63-66`). The templated analogue must replace `exp` with
  `Dal::AAD::exp`, the forward integration with a `T_`-typed accumulation over the
  PWL `fLeft_`/`fRight_` parameters, and the base read with a `T_`-typed
  `(*base_)(from, to)` when a base is supplied.
- `CurveWithBase_<class T_, class B_ = T_>` (`dal-cpp/dal/curve/yccomponent.hpp:24-49`)
  IS templated and already used by `Tape::DiscountLogDF_<T_>` with a double-typed
  base; under Option B the new `Tape::DiscountPWLF_<T_>` inherits
  `CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<T_>>` when base-layered and
  `CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<double>>` (or no base) otherwise.

**User LOCKED Option B.** The AAD path extends to `PIECEWISE_LINEAR_FWD` via a new
`Tape::DiscountPWLF_<T_>`. Why Option A (LOG_DISCOUNT-only) was rejected: the joint
module's native parameterization IS `PIECEWISE_LINEAR_FWD`, the joint validator
already supports it, the joint example already uses it, and forcing every joint caller
onto `LOG_DISCOUNT` would (a) diverge from the module default and the example,
(b) require the joint validator to relax its `knotDates_.front() > today_` rule to
`== today_` for LOG_DISCOUNT (a validation-rule fork), and (c) require the
`ParamsPerKnot(LOG_DISCOUNT) == REQUIRE(false)` arm at `jointcalibration.cpp:42-44`
to be opened up. Option B avoids all three by AAD-ing the parameterization the joint
module already ships.

**Spec consequence (Option B):** the joint AAD path authors
`Tape::DiscountPWLF_<T_>` as the templated PWL-forward curve, with a templated base
handle. Its arithmetic body mirrors the double `DiscountPWLF_::operator()` at
`ycimp.cpp:63-66` with `T_`-typed forward integration and `Dal::AAD::exp`. The
PWL_FWD parameter vector has **2 params/knot with NO anchor pinning** (every knot,
including knot 0, is free) — the critical structural difference from LOG_DISCOUNT
(where knot 0 is the anchor and the free vector is `nKnots - 1`). The independent
registration in `AnalyticJacobian` therefore registers all `2 * nKnots` parameters of
every declaration (no anchor exclusion), and the column map is solver col `j` =
declaration `d`'s storage parameter `j` directly.

### Gap 3 — Projection-curve fixings: a NEW `Tape::JointRate_<T_>` base (CP3, critique B4 RESOLVED)

Phase A's eligibility EXCLUDES projection-curve instruments
(`calibration.cpp:467-473`: `useProjectionCurve_ == true` → NOTICE → fallback). The
joint IBOR forward declarations REQUIRE them — the joint validator
(`jointcalibration.cpp:225-231`) REQUIRES `useProjectionCurve_ == true` on every
forward-declaration instrument, because a mis-conventioned IBOR leg would fix off the
discount curve and leave the forward curve unconstrained (BAR-C, the validator's
stated rationale). So Phase A's templated rates CANNOT be reused for the joint
IBOR projection slice (the OIS-discount slice, where `forecast == discount`, still
rides Phase A's `Swap_::PrecomputeT<T_>` — see Gap 5).
Source-verified (this pass):

- `Tape::Rate_<T_>::operator()` is declared `virtual T_ operator()(const YCCtx_<T_>& ctx) const = 0;`
  at `ycinstrument.hpp:27` — the virtual is BOUND to `YCCtx_<T_>`.
- `Tape::YCCtx_<T_>` (`ycctx.hpp:18-22`) carries exactly ONE `const DiscountCurve_<T_>& curve_`
  and has no `Forward(...)`.
- All four Phase A templated rate bodies — `Tape::DepositRate_<T_>` (`ycinstrument.cpp:283`),
  `Tape::ForwardRate_<T_>` (`:311`), `Tape::SwapRate_<T_>` (`:347-369`) — read ONLY
  `ctx.curve_`; none calls any forecast curve. The `SwapRate_<T_>` comment at `:348-350`
  states the assumption explicitly: "Phase A eligibility guarantees forecast == discount
  == ctx.curve_".
- `Swap_::PrecomputeT<T_>()` (`ycinstrument.cpp:494-506`) returns a
  `Handle_<Tape::Rate_<T_>>` wrapping a `Tape::SwapRate_<T_>` whose `operator()` reads
  only `ctx.curve_`. So the inherited templated rate is **numerically wrong for any joint
  path where forecast != discount** — including vanilla IBOR swaps in a multi-curve system.
- `PhaseARateAt<T_>` (`calibration.cpp:499-510`) dispatches via `dynamic_cast` and invokes
  the rate with a `YCCtx_<Number_>` (`:548`, `:555`).

**Spec consequence (CP3 LOCKED):** the joint AAD path authors a NEW templated rate
base — `Tape::JointRate_<T_>` — whose pure virtual is
`virtual T_ operator()(const JointCurveBlock_<T_>& block) const = 0;` (the
`JointCurveBlock_<T_>` routing context is the Gap 1 deliverable). This base lives in
the joint module's anonymous namespace (or a joint-local header) and is a SIBLING of
Phase A's `Tape::Rate_<T_>`, NOT a modification. Phase A's `Tape::Rate_<T_>` virtual,
its `YCCtx_<T_>` context, and its four rate subclasses are UNTOUCHED (NG2). The four
projection-capable subclasses (`Tape::DepositRateProj_<T_>`, `Tape::ForwardRateProj_<T_>`
covering FRA and Future, `Tape::SwapRateProj_<T_>`) are constructed directly from each
instrument's schedule via a joint-local `ProjectionRateAt<T_>(d, i)` dispatch in
`JointResidualFunction_` — they do NOT route through `Swap_::PrecomputeT<T_>()` or any
existing `PrecomputeT`. The arithmetic bodies mirror the double-path rate bodies at
`ycinstrument.cpp:73-251` with `T_` for DF reads and rate accumulations; dcf, yf,
fixing dates, convexity adjustments stay `double`.

The joint residual performs BOTH reads in the `Number_` domain:

- a DISCOUNT read at the leg's collateral (resolves to the discount declaration's
  templated curve), and
- a FORECAST read at the leg's `(forecastTenor_, collateral_)` (resolves to the
  forward declaration's templated curve when registered, else the discount curve —
  matching `CurveBlock_::Forward`'s fallback at `curveblock.cpp:76-83`).

The instrument types in scope for the joint AAD path are exactly those Phase A covers
— `Deposit_`, `FRA_`, `Future_`, `Swap_` — INCLUDING `OISSwap_` (which inherits
`Swap_` and rides `Swap_::PrecomputeT<T_>` on the OIS-discount slice where
`forecast == discount`; see Gap 5), extended to read a forecast curve when their
`RateIndexConvention_::useProjectionCurve_` is true. **Instrument types explicitly
OUT of scope:** `BasisSwap_` (no Phase A templated rate; the double-path
`BasisSwapRate_` resolves TWO forecast curves) and `STIR_`. The eligibility predicate
(FR3) rejects any declaration containing an out-of-scope instrument type and falls
back to bumped.

### Gap 4 — Base handle: OIS sensitivity through the forward curve's base

Base-layered forward declarations carry `base = jointOis`
(`JointCurveDeclaration_::baseLayeredOverDiscount_`, `jointcalibration.hpp:46`; wired
at `jointcalibration.cpp:336-339` and `:481-484` in the double path). An OIS knot
perturbation must flow into the 3M forward curve's discount factors THROUGH that base
handle — the whole point of base layering is that the smoother acts on the spread
forward `f_abs - f_ois`, so the OIS delta reaches the forward curve via its base, not
just via the IBOR discounting leg. The double path gets this for free via
`NewDiscountPWLF(..., base)` returning a `DiscountPWLF_` whose
`CurveWithBase_<DiscountCurve_>` base is the OIS curve (`ycimp.cpp:63-66`). The AAD
path needs the templated equivalent: the forward declaration's templated curve
(`Tape::DiscountPWLF_<T_>`) must hold a `T_`-typed reference to the discount
declaration's templated curve built in the SAME `Gradient` call, so the reverse
sweep propagates OIS adjoints into the forward-curve parameters through the base.
Confirmed against source:

- `CurveWithBase_<T_, B_>` is templated (`yccomponent.hpp:24-49`) and already used
  by `Tape::DiscountLogDF_<T_>` with `B_ = DiscountCurve_<double>` — but in Phase A
  the base is a double-typed curve treated as a CONSTANT from the tape's perspective
  (`calibration.cpp:247-256`). That is correct for Phase A (no base layering; the
  single base curve is exogenous). For the joint path the base MUST be the
  `T_`-typed discount curve built in the same sweep, so its adjoints propagate.

**Spec consequence (Option B):** the new `Tape::DiscountPWLF_<T_>` inherits
`CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<T_>>` when a base is supplied,
and its `operator()` multiplies the self-integrated `T_`-typed log-DF by the
`T_`-typed base read `(*base_)(from, to)`. With a `T_`-typed base that multiplication
records on the tape and the reverse sweep propagates into the base curve's parameters
(the OIS discount-curve free nodes). The baseless variant is the same class with a
null/empty base handle (mirroring the double `DiscountPWLF_` at `ycimp.cpp:65`, where
`base_ ? (*base_)(from, to) : 1.0`). Either way, the eligibility predicate (FR3)
must require that every base-layered forward declaration's `targetCollateral_` is
produced by a discount declaration in the SAME spec whose parameterization is also
AAD-eligible (so the base is itself a `T_`-typed curve, not a double constant). A
base-layered forward declaration whose base collateral is not produced by an eligible
discount declaration falls back to bumped.

**Base-handle ownership (critique S6 RESOLVED):** `Handle_<T_>` is
`std::shared_ptr<const T_>` (`externals/machinist/include/machinist/handle.hpp:38-43`)
and `CurveWithBase_<T_, B_>::base_` is a `Handle_<B_>` (`yccomponent.hpp:26-27`) that
participates in `Poll`, `NewBase`, and the subclass `Clone` plumbing. So the templated
base inside `Tape::DiscountPWLF_<T_, B_>` is OWNED via `shared_ptr`. The `AnalyticJacobian`
frame's `curveStorage` MUST therefore be `std::vector<std::shared_ptr<Tape::DiscountCurve_<Number_>>>`
(not `unique_ptr`), so the base-layered forward declaration's `Handle_<DiscountCurve_<Number_>>`
can share ownership of the discount declaration's curve for the duration of the sweep.
The non-owning `const DiscountCurve_<Number_>*` pointers inside `JointCurveBlock_<Number_>`
(Gap 1) remain non-owning — they alias the same storage. This avoids any change to the
existing `CurveWithBase_<T_, B_>` template (which Phase A's `Tape::DiscountLogDF_<T_>`
also uses with a double base), preserving NG2.

### Gap 5 — OIS overnight compounding: NON-ISSUE (CP4 RETIRED 2026-06-20)

The second-pass spec (CP4, critique B5) asserted that `OISSwap_`'s inherited
`Tape::SwapRate_<T_>::operator()` prices the overnight leg with arithmetic
single-period fixings while a "correct" OIS leg would compound overnight
geometrically, making the Jacobian wrong, and on that basis rejected `OISSwap_` for
the ANALYTIC path. **That concern does not exist in this codebase.** The lead
re-verified `dal-cpp/dal/curve/ycinstrument.cpp` this pass:

- `Tape::SwapRate_<T_>::operator()` (`ycinstrument.cpp:347-368`) prices each float
  period as `fixing * dcf * DF` with
  `fixing = ForwardRate(discount, accrualStart, accrualEnd, basis, ctx)`.
- The double path prices each period with the SAME `ForwardRate`
  (`ycinstrument.cpp:53-59`: `(1.0 / fwdDf - 1.0) / basis(...)`), accumulated the same
  way (`ycinstrument.cpp:187-196`).
- A grep for `compound` / `geometric` / `overnight` across `dal-cpp/dal/curve/` finds
  NO daily-compounding loop and NO closed-form OIS accumulator — only the
  `overnightConvention` parameter name (`ycinstrument.cpp:525`).

So AAD and bumped evaluate the IDENTICAL function on OIS rows: the AAD-vs-bumped
oracle (AC1) PASSES for OIS, and there is no arithmetic-vs-geometric convexity gap in
the Jacobian, because both paths share the same simple-rate arithmetic. The "geometric
compounding" B5 worried about is a library-wide modeling simplification that affects
AAD and bumped identically — orthogonal to this feature.

Routing confirms eligibility: `OISSwap_`'s overnight index has
`useProjectionCurve_ == false` (`ycinstrument.cpp:44`), so it fixes off the discount
curve (`forecast == discount == OIS`), and the inherited single-curve
`Swap_::PrecomputeT<T_>` (which returns a `Tape::SwapRate_<T_>` reading `ctx.curve_`)
is BOTH routing-compatible AND gradient-correct for OIS. `OISSwap_` runs ANALYTIC via
the inherited machinery; `Tape::JointRate_<T_>` (Gap 3 / CP3) is needed ONLY for the
IBOR(3M) projection slice where `forecast(3M) != discount(OIS)`.

**Spec consequence (CP4 RETIRED):** `OISSwap_` is ELIGIBLE for the joint ANALYTIC
path. The FR3 (e) `dynamic_cast<const OISSwap_*>` rejection is REMOVED. The example
program's OIS slice stays `OISSwap_` (`joint_multi_curve_calibration.cpp:163-168`) and
exercises the AAD path natively — NO instrument-type edit. `Tape::OisSwapRate_<T_>` is
not needed (NG10). The second-pass OQ-1 / OQ-8 B5 closes are SUPERSEDED — see Open
Questions.

## Functional Requirements

- **FR1 — Override returns a real Jacobian when engaged.**
  `JointResidualFunction_::Gradient(x, f)` returns a non-null
  `Underdetermined::Jacobian_*` whose dense `Matrix_<>` has shape
  `(totalResiduals) x (totalFreeParams)` when `jacobianMode_ == ANALYTIC` AND the
  cached joint-eligibility verdict is `Eligible`; otherwise it returns `nullptr` and
  the solver dense-bumps (unchanged numerics). The returned object implements every
  pure virtual of `Underdetermined::Jacobian_`
  (`dal-cpp/dal/math/optimization/underdetermined.hpp:46-57`). Whether the joint path
  ports `XCurveJacobian_` verbatim, defines its own struct, or factors the dense
  subclass into a shared header is a design decision (see Open Question OQ-2).

- **FR2 — Single-result reverse sweep, backend-neutral.**
  The Jacobian is computed by the same single-result reverse-sweep loop Phase A uses
  (`Clear → RegisterIndependent ×N → NewRecording → forward eval → per-row
  {ZeroAdjoints, Adjoint=1, PropagateToStart, harvest}`), running identically on all
  four backends via the `Dal::AAD` facade. Recording-contract order is authoritative
  per `aad-analytic-jacobian-redesign.md` "Implementation realities" item 1; violating
  it silently yields an all-zero Jacobian on XAD. `Dal::AAD::Value` is used to extract
  doubles from `Number_` (NEVER `static_cast<double>(Number_)`, which does not compile
  on XAD/Adept — see user memory on `static_cast<double>(Number_)` portability). The
  recording contract that works on all four backends is:
  `Clear → RegisterIndependent (per free parameter) → NewRecording → forward
  evaluation (build every `T_`-typed curve, assemble the `T_`-typed routing context,
  compute stacked residuals) → for each residual row { `ZeroAdjoints` →
  `Adjoint(residual[i]) = 1.0` → `PropagateToStart` → harvest `Adjoint(param[j])` }`.

- **FR3 — Joint eligibility predicate (joint analogue of Phase A's
  `EligibleForAnalyticJacobian`).** The predicate is a pure query over ctor-stored
  state, NEVER throws, and returns `true` iff ALL of:
  - (a) `jacobianMode_ == ANALYTIC`;
  - (b) the build is the native backend OR the templated machinery has been verified
    to compile and run under the active external backend (per Goal G2 and the
    acceptance criteria; the compile-time gate is the same shape as Phase A's, see
    OQ-3);
  - (c) every declaration's `parameterization_` is `PIECEWISE_LINEAR_FWD`
    (Option B). Other parameterizations (`LOG_DISCOUNT`, `PIECEWISE_CONSTANT_FWD`,
    `ZERO_RATE`) on a joint declaration trigger a NOTICE naming the declaration index
    and the offending parameterization and fall back to bumped. (Note: `LOG_DISCOUNT`
    remains AAD-eligible on the single-curve path via Phase A; it is out of scope for
    the JOINT AAD path under Option B's first cut — NG9.);
  - (d) every declaration's `solveMode_` contribution is consistent with the joint
    `solveMode_` (the joint solve has ONE `solveMode_` on the spec, so this collapses
    to: `spec.solveMode_ == EXACT` for the forward-Jacobian capture path; ANALYTIC
    still engages for APPROXIMATE but produces no at-solution forward J — mirrors
    Phase A at `calibration.cpp:824`);
  - (e) every instrument in every declaration is one of
    `{Deposit_, FRA_, Future_, Swap_}` — which INCLUDES `OISSwap_` (`OISSwap_ :
    public Swap_`, `ycinstrument.hpp:155`; see Gap 5). `OISSwap_` rides the inherited
    `Swap_::PrecomputeT<T_>`: its overnight index has `useProjectionCurve_ == false`
    (`ycinstrument.cpp:44`) so `forecast == discount == OIS`, and the inherited
    `Tape::SwapRate_<T_>::operator()` (`ycinstrument.cpp:347-368`) uses the identical
    simple-rate `ForwardRate` arithmetic as the double path (`ycinstrument.cpp:53-59`)
    — so the Jacobian is correct (AAD and bumped evaluate the same function; AC1
    passes for OIS). `BasisSwap_`, `STIR_`, and anything outside the four-type set
    trigger ineligibility. A declaration containing any rejected type makes the WHOLE
    joint solve fall back to bumped (the path is all-or-nothing);
  - (f) for every forward declaration, every instrument has
    `RateIndexConvention_::useProjectionCurve_ == true` (the joint validator already
    `REQUIRE`s this at `jointcalibration.cpp:225-231`, so this is structurally
    guaranteed for valid specs — but the eligibility predicate still names it in the
    NOTICE for clarity);
  - (g) for every discount declaration OR baseless forward declaration, every
    instrument has `useProjectionCurve_ == false` (mirrors Phase A's
    forecast==discount rule for the discount curve); a forward declaration that
    projects through its own calibrated forward curve is fine, but a discount
    declaration instrument that projects would route off a not-yet-built forward
    curve and is ineligible;
  - (h) every base-layered forward declaration's `targetCollateral_` is produced by
    a discount declaration in the same spec whose parameterization is also
    AAD-eligible (Gap 4); a base-layered forward whose base collateral is not
    produced by an eligible PWL_FWD discount declaration falls back to bumped.
    NOTE (critique S10): this clause is structurally guaranteed — the joint validator
    at `jointcalibration.cpp:219-223` `THROW`s on a missing base collateral BEFORE
    the eligibility predicate runs, and clause (c) already requires every declaration
    is PWL_FWD. So the (h) NOTICE is UNREACHABLE in practice; it is retained for
    symmetry with the FR3 list and marked "defensive, unreachable" in the NOTICE
    contract;
  - (j) every declaration's `liborBasis_` (and the day-count used by the templated
    `Tape::DiscountPWLF_<T_>::operator()` denominator) is consistent. The double
    `DiscountPWLF_::operator()` uses a HARDCODED `365.0` literal (`ycimp.cpp:65`),
    NOT a `dayBasis_`/`liborBasis_` member — the class holds only a `PiecewiseLinear_`
    and consults no day-count. A `DAYS_PER_YEAR = 365.0` constant exists at
    `ycconst.cpp:16` (used by `DiscountPWC_`, NOT by `DiscountPWLF_`). The joint
    default `liborBasis_ = DayBasis_("ACT_365F")` (`jointcalibration.hpp:60`) happens
    to agree with the `365.0` literal. The templated twin MUST route through the
    `DAYS_PER_YEAR` constant (so it is a compile-time `double`, safe for `T_`
    derivatives) and the eligibility predicate REJECTS any spec whose
    `liborBasis_ != ACT_365F` with a one-time NOTICE and bumped fallback. Non-default
    day bases (ACT/360, 30/360) are out of scope for the joint AAD path under Option B's
    first cut (critique S7). Fixing the underlying day-count inconsistency in the
    double `DiscountPWLF_` itself is also out of scope (NG2).
  - (i) every instrument's `TradeDate()` equals its declaration's knot 0 (the joint
    analogue of Phase A's `inst->TradeDate() == knotDates_.front()` at
    `calibration.cpp:479-485`); a spot-started instrument with `tradeDate != knot 0`
    would silently misprice its residual row on the tape. NOTE: under Option B the
    anchor is `knotDates_.front()`, which the joint validator REQUIRES to be
    `> today_` (`jointcalibration.cpp:203-204`) — the PWL_FWD parameterization does
    NOT pin knot 0 at `today_` (unlike LOG_DISCOUNT), so the tradeDate==anchor rule
    is `tradeDate == knotDates_.front()`, not `tradeDate == today_`. This dissolves
    the LOG_DISCOUNT-only anchor-vs-validation tension that Option A would have
    created (see the B3 close in Open Questions).

  Each failed clause emits a `NOTICE` naming the offending declaration index,
  instrument name (when applicable), and the failing condition. The verdict is
  evaluated EXACTLY ONCE per `JointResidualFunction_` lifetime and cached
  (`Eligibility_{Unknown, Eligible, Ineligible}` member), so every `NOTICE` fires at
  most once per `CalibrateJointMultiCurve` call — mirroring Phase A's H1
  once-per-call NOTICE budget (`calibration.cpp:282-289, 396-400`).

- **FR4 — Per-call mode flag, default ANALYTIC.**
  A `CurveJacobianMode_ jacobianMode_` is exposed on a per-call joint options surface
  (sibling of `JointMultiCurveCalibrationSpec_`, NOT on the spec or the declaration —
  mirroring Phase A's H2). **Default `ANALYTIC`**, matching the single-curve default
  (`dal-cpp/dal/curve/calibration.hpp:102`). `CalibrateJointMultiCurve` gains a
  two-arg overload; the existing single-arg overload delegates with a
  default-constructed options → `ANALYTIC`. Existing callers therefore exercise the
  AAD path by default after the upgrade; the migration note states this explicitly.
  The shape of the options struct (a new `JointMultiCurveCalibrationOptions_` vs
  reusing `CurveCalibrationOptions_` vs threading the mode as a sibling arg) is a
  design decision for `dal-api-designer`; the spec pins only the contract: per-call,
  default `ANALYTIC`, NOT serialized, NEVER throws.

- **FR5 — Templated joint residual.**
  The whole residual computation — every declaration's curve build from its `x`
  slice (as `Tape::DiscountPWLF_<T_>` under Option B), the multi-curve routing
  assembly (Gap 1), the IBOR(3M) projection slice's forecast AND discount reads via
  the NEW `Tape::JointRate_<T_>` hierarchy and `JointCurveBlock_<T_>` context (Gap 3)
  while the OIS-discount slice rides the inherited `Swap_::PrecomputeT<T_>`
  (`forecast == discount`, Gap 5), and the base-handle propagation for base-layered
  forward declarations (Gap 4) — has a `template<class T_>` counterpart producing a
  `Vector_<T_>` of stacked residuals.
  The `double`-typed `F` path (`jointcalibration.cpp:311-354`) is unchanged; the
  templated path lives alongside it and is exercised only inside the AAD Jacobian.
  The two-pass build order (discount declarations first, then base-resolving forward
  declarations) is preserved on the templated path so a base-layered forward resolves
  its base from a discount curve built in the SAME sweep, regardless of declaration
  order. The `Tape::DiscountPWLF_<T_, B_>` class holds FLAT `Vector_<T_> fLeftT_,
  fRightT_` members and computes the `T_`-typed running integral `sofarT_`
  (`Vector_<T_>`, mirroring the double `PiecewiseLinear_::sofar_` at
  `piecewiselinear.hpp:16`) inline — NOT a templated `PiecewiseLinearT_<T_>` helper
  (critique S9: the joint path is the only consumer, so flat members minimize surface).
  The `T_`-typed `IntegralTo(t)` reproduces all FOUR branches of the double
  `PiecewiseLinear_::IntegralTo` (`piecewiselinear.cpp:23-37`: extrapolation below
  the first knot, flat-forward beyond the last knot, on-a-knot shortcut, in-range
  partial trapezoid) with `double` abscissa weights and `T_` forward values, and uses
  the `fLeftT_[ii] + fRightT_[ii-1]` segment indexing verbatim from the double
  `Sofar()` (`piecewiselinear.cpp:17`) — an off-by-one here would compile and produce
  a structurally-wrong Jacobian (critique S8). The `ApplyDX` override compiles on
  `T_ = Number_` (via the facade's `+=` and `leverage * double` operators, mirroring
  Phase A's `Tape::DiscountLogDF_<T_>::ApplyDX` at `yclogdf.cpp:419-430`) but is
  UNREACHABLE on the AAD path: the `Number_` factory constructs the curve directly
  with tape-registered `fLeftT_`/`fRightT_`, never calling `ApplyDX`. The bumped
  fallback uses the existing double `DiscountPWLF_` (anonymous namespace,
  `ycimp.cpp:56-83`), NOT the templated `Tape::DiscountPWLF_<double>`, so the
  templated `ApplyDX` is never exercised at runtime (critique S9).

- **FR6 — ANALYTIC never throws.** `jacobianMode_ == ANALYTIC` on an ineligible
  joint spec routes to bumped with a one-time NOTICE per failed clause. No `REQUIRE`
  or `THROW` on the ineligibility path. (Mirrors Phase A's H3 / M5.)

## Non-Functional Requirements

- **Performance.** Target workload: a joint system with 1 discount + 1–3 forward
  declarations, 8–30 instruments per declaration, 10–30 knot dates per declaration.
  The single-result AAD Jacobian must NOT be slower than the dense-bumped Jacobian
  the solver would otherwise compute (i.e. `(P+1) × cost(F)` where `P` is total free
  params). Measurement: the `solverEvaluations_` field on
  `JointMultiCurveCalibrationResult_` (`jointcalibration.hpp:93`) plus an
  example/test timing comparison. The multi-result speedup is explicitly deferred
  (NG1); no hard latency target is set in Phase B beyond "not slower than bump."
- **Differentiability.** Every discount-factor read and every forecast-rate read the
  joint residual performs in the double path has a `Number_`-typed counterpart on the
  AAD path (Gap 1, Gap 3). Every base-layered forward declaration's base is a
  `Number_`-typed curve built in the same sweep (Gap 4). The forward-to-log-DF
  integration inside `Tape::DiscountPWLF_<T_>` is `T_`-typed end-to-end so the tape
  records the dependence on every free-node `fLeft`/`fRight` parameter. The reverse
  sweep produces exact structural zeros where the bump would produce near-zero noise
  — one of the wins the AAD path delivers and the FD oracle test verifies.
- **Compatibility.**
  - The default-constructed joint options (`ANALYTIC`) now exercises the AAD path on
    eligible specs. On an INELIGIBLE spec (NOTICE fall-through), the bumped result
    is byte-for-byte identical to the current `CalibrateJointMultiCurve(spec)`
    single-arg call. The existing single-arg overload is preserved unchanged.
  - No existing joint calibration test, example, or public struct field is renamed
    or removed. `JointCurveDeclaration_` and `JointMultiCurveCalibrationSpec_` gain
    NO new fields in Phase B (the mode lives on the options surface).
  - The single-curve `CalibrateYieldCurve` AAD path and `Tape::DiscountLogDF_<T_>`
    are untouched (Option B introduces a sibling `Tape::DiscountPWLF_<T_>`, not a
    modification).
  - `CurveJacobianMode_` already exists (revived two-value in Phase A); Phase B
    reuses it without enum changes, so NO Machinist regen is required for the flag.
- **Backend coverage.** Build + run + test under all four CMake presets
  (`build/` native, `build-adept/`, `build-xad/`, `build-codi/`). Run binaries from
  `build/<sub>/`, NEVER from `bin/` (which goes stale via `make install`).
- **Coding style.** Follow `.codex/skills/dal-agent-team/references/code-style.md` (4-space indent, 150-col,
  PascalCase + trailing `_`, `T*` pointer binding, anonymous-namespace helpers,
  `template<class T_>` AAD idiom). Follow `.codex/skills/dal-agent-team/references/unit-test-style.md` for the
  new tests. Machinist-generated enums are never hand-edited.

## Inputs and Outputs

| Name                          | Type                                   | Units / Domain                                | Range / Constraints                                                                                                         |
|-------------------------------|----------------------------------------|-----------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `x` (input to `Gradient`)     | `const Vector_<>&`                     | joint free-parameter vector                   | length = sum over declarations of `ParamsPerKnot(param) × nKnots_decl` (PWL_FWD: `2 × nKnots_decl` per decl — every knot free) |
| `f` (input to `Gradient`)     | `const Vector_<>&`                     | stacked residuals at `x`                      | length = total instruments across all declarations                                                                          |
| `Gradient` return             | `Underdetermined::Jacobian_*`          | dense Jacobian `dResidual_i / dParam_j`       | non-null iff `ANALYTIC && Eligible`; shape `(totalResiduals) x (totalFreeParams)`; caller owns the `new`-ed ptr             |
| `jacobianMode_` (options)     | `CurveJacobianMode_`                   | `{BUMPED, ANALYTIC}`                          | default `ANALYTIC`                                                                                                          |
| Eligibility verdict (cached)  | `Eligibility_` (internal enum)         | `{Unknown, Eligible, Ineligible}`             | evaluated once per `JointResidualFunction_` lifetime                                                                        |
| NOTICEs (side effect)         | `Dal::Exception` stack via `NOTICE`    | human-readable strings                        | at most once per failed clause per `CalibrateJointMultiCurve` call                                                          |

## Acceptance Criteria

- [ ] **AC1 — AAD-vs-bumped Jacobian agreement (the definitive correctness check).**
  Given a small joint system (1 OIS-discount declaration with 3–5 `OISSwap_`
  instruments + 1 3M-IBOR forward declaration with 3–5 IBOR swaps/fras, base-layered
  over OIS), when `Gradient` is called at a non-trivial `x`, then the returned AAD
  Jacobian agrees
  element-wise with a central finite-difference bump (`bump = 1e-6`) of the joint `F`
  to a tight tolerance (`ASSERT_NEAR(aad, fd, 1e-6)` relative per element, i.e.
  `|aad - fd| / max(1, |fd|) < 1e-6`). The FD oracle is backend-independent, so an
  all-zero AAD row against a non-zero FD row fails loud. Run under EACH of `build/`,
  `build-adept/`, `build-xad/`, `build-codi/`.
- [ ] **AC2 — Per-row non-trivial-Jacobian invariant (Phase A's B2 sentinel).** For
  every residual row `i`, at least one parameter column `j` has `|jac(i, j)| > 1e-6`.
  An all-zero row is the signature of a missed `registerInput` or broken recording
  window; this invariant trips it before FD runs.
- [ ] **AC3 — Cross-row cleanliness invariant (Phase A's B1 sentinel).** Seed a
  joint problem where the OIS residual rows and the IBOR residual rows depend on
  disjoint parameter sub-vectors OIS knots vs IBOR knots. Assert row `i`'s harvested
  Jacobian is identical whether row `i-1` was swept first or not (run the loop twice
  with row order swapped; assert element-wise equal to `1e-12`). Under a stale
  `ZeroAdjoints`, the second ordering would carry the first's operand residue and
  differ — the direct falsifier for the Adept-no-op class.
- [ ] **AC4 — Cross-curve coupling captured.** A perturbation of an OIS discount
  knot produces a non-zero sensitivity in at least one IBOR forward residual row
  (via the IBOR leg's discounting) AND a perturbation of an IBOR forward knot
  produces a non-zero sensitivity in at least one IBOR residual row (via the
  forecast read). Both are asserted non-zero in the AAD Jacobian AND match FD. This
  is the test that proves Gap 1 + Gap 3 are wired correctly.
- [ ] **AC5 — Base-handle propagation.** With a base-layered forward declaration,
  a perturbation of an OIS discount knot produces a non-zero sensitivity in the
  forward declaration's own residual rows THROUGH the base handle (i.e. the
  forward-curve residual is sensitive to OIS knots beyond just the IBOR
  discounting channel). Asserted non-zero in AAD AND matches FD. This is the test
  that proves Gap 4 is wired correctly (the `Tape::DiscountPWLF_<T_>` templated base
  propagates adjoints into the OIS discount curve).
- [ ] **AC6 — Four-backend engagement.** AC1–AC5 pass under each of native, XAD,
  CoDiPack, and Adept (run from `build/`, `build-xad/`, `build-codi/`,
  `build-adept/` respectively). The `SKIP_IF_NO_ANALYTIC_JACOBIAN()` macro from
  Phase A is NOT used for the joint suite — the joint AAD path must run, not skip,
  on every backend.
- [ ] **AC7 — Eligibility NOTICEs fire once.** For each ineligibility clause of FR3
  (non-PWL_FWD parameterization; out-of-scope instrument type `BasisSwap_` / `STIR_`
  (`OISSwap_` is ELIGIBLE — see Gap 5); projection-curve violation on a discount
  declaration; missing base-collateral discount declaration (defensive / unreachable
  per S10); tradeDate != knot 0; non-ACT/365F `liborBasis_`), an `ANALYTIC` joint
  spec emits the expected `NOTICE` exactly once across a full
  `CalibrateJointMultiCurve` run, then falls back to bumped and converges to the
  bumped result within solver tolerance. (Phase A verified this structurally rather
  than by counting NOTICEs; the joint path inherits that approach —
  `aad-analytic-jacobian-redesign.md` "Implementation realities" item 4.)
- [ ] **AC8 — BUMPED fallback is byte-for-byte.** A joint options constructed with
  `jacobianMode_ = BUMPED` produces a `CalibrateJointMultiCurve` result identical
  (calibrated node values within solver tolerance, `solverEvaluations_` in the same
  ballpark) to the current single-arg call on the same spec. Existing joint
  calibration tests pass unchanged (the default flipped to `ANALYTIC`, but every
  existing test spec that was eligible is now exercised on the AAD path — those
  tests are UPDATED in the implementation step to assert the AAD result matches the
  bumped result, not to pin BUMPED).
- [ ] **AC9 — No regression.** The full `bin/dal_cpp_tests` (built fresh in
  `build/`) is green, with no regressions in the existing joint calibration suite,
  the single-curve analytic Jacobian suite, or any other test. The four-backend
  build matrix all compile (AC6 covers the run).
- [ ] **AC10 — Code style + reviewer pass.** All changed files pass
  `dal-reviewer` / `dal-code-style-review`. No hand-edited Machinist enums. No
  `static_cast<double>(Number_)`. Anonymous-namespace helpers where Phase A uses
  them. `Dal::AAD::Value` for double extraction. The new `Tape::DiscountPWLF_<T_>`
  follows the Phase A `Tape::DiscountLogDF_<T_>` header/source layout
  (`dal-cpp/dal/curve/yclogdf.hpp` / `yclogdf.cpp`).
- [ ] **AC11 — Templated PWL arithmetic byte-for-byte (critique S8 falsifier).** On a
  single-declaration PWL_FWD discount curve with a discontinuity at EVERY knot (so the
  `fLeftT_[ii] + fRightT_[ii-1]` segment indexing is exercised), the `double`
  specialization `Tape::DiscountPWLF_<double>::operator()(from, to)` matches the
  existing anonymous-namespace double `DiscountPWLF_` (`ycimp.cpp:63-66`)
  element-wise to `1e-15` across query intervals that hit all four `IntegralTo`
  branches (below first knot, beyond last knot, on-a-knot, in-range partial
  trapezoid). This isolates the templated PWL arithmetic from the AAD layer and is
  the cheapest falsifier for the off-by-one class.

## Open Questions

- **OQ-1 (`OISSwap_` coverage — RESOLVED, CP4 retired).** The first-pass spec
  asserted "`OISSwap_` rides `Swap_::PrecomputeT<T_>()`". The second-pass re-critique
  (B5) SUPERSEDED this with CP4 (reject `OISSwap_` on a geometric-compounding
  concern). The third pass RETIRES CP4 and RESTORES the first-pass resolution: the
  lead re-verified `ycinstrument.cpp` and found NO geometric overnight compounding in
  the library — the AAD path (`Tape::SwapRate_<T_>::operator()`,
  `ycinstrument.cpp:347-368`) and the double path (`ForwardRate`,
  `ycinstrument.cpp:53-59`) use the IDENTICAL simple-rate arithmetic, and the OIS
  overnight index has `useProjectionCurve_ == false` (`ycinstrument.cpp:44`) so
  `forecast == discount == OIS`. The inherited `Swap_::PrecomputeT<T_>` is therefore
  both routing-compatible and gradient-correct for OIS (AC1 passes). `OISSwap_` is
  ELIGIBLE for the joint ANALYTIC path; the example's OIS slice stays `OISSwap_`.
  See Gap 5 for the full verification.
- **OQ-2 (Jacobian subclass reuse).** `XCurveJacobian_`
  (`calibration.cpp:43-81`) is structurally identical to what the joint path needs
  but is TU-private. Options: (a) port it verbatim into `jointcalibration.cpp`'s
  anonymous namespace; (b) factor it into a shared header (e.g.
  `dal-cpp/dal/curve/curvejacobian.hpp`) and have both TUs include it; (c) define a
  joint-local variant. Flag for `dal-api-designer`. (The earlier critique blocker B2
  re: `BuildJointSmoothing` free-knot iteration is re-derived and closed under
  PWL_FWD below — it does NOT apply to Option B.)
- **OQ-3 (compile-time gate).** Phase A's `#if !defined(DAL_USE_XAD_AAD) && ...`
  gate was DROPPED in the Phase A ship (the runtime enum + backend-neutral facade
  replaced it, per Decision 6 of `aad-analytic-jacobian-redesign.md`). Should the
  joint AAD path ship with NO compile-time gate (matching shipped Phase A), or carry
  a belt-and-suspenders gate for one release until four-backend CI is green? The spec
  assumes NO gate (matching shipped Phase A); flag for the architect.
- **OQ-4 (options struct shape).** New `JointMultiCurveCalibrationOptions_` vs reuse
  `CurveCalibrationOptions_` vs sibling `CurveJacobianMode_` arg. The spec pins only
  the contract (per-call, default ANALYTIC, NOT serialized, NEVER throws); the shape
  is `dal-api-designer`'s. Recommend a new joint options struct for symmetry with the
  joint spec/result/diagnostics trio and to leave room for future joint-specific
  solver knobs.
- **OQ-5 (templated rate design — RESOLVED by CP3, critique B4).** For Gap 3, the
  cleanest extension of the Phase A templated rates to projection-curve instruments.
  Three options were on the table: (i) new `Tape::` projection-capable rate
  subclasses behind a NEW `Tape::JointRate_<T_>` base; (ii) broaden the existing
  `Tape::Rate_<T_>` subclasses to take either a `YCCtx_<T_>` or a
  `JointCurveBlock_<T_>`; (iii) templated `ResolveForecastCurve`/`ForwardRate` pair
  reading off a templated forecast source. **RESOLUTION (CP3, LOCKED this pass):
  option (i).** The source verification confirms `Tape::Rate_<T_>::operator()` is
  bound to `const YCCtx_<T_>&` (`ycinstrument.hpp:27`), so options (ii) and (iii)
  would either broaden the Phase A virtual (regression risk on shipped Phase A, NG2)
  or bypass it (the "OISSwap_ rides Swap_" claim is correct for the OIS-discount
  slice where `forecast == discount`, but the IBOR projection slice genuinely needs a
  new base — see Gap 5). The new `Tape::JointRate_<T_>` base declares
  `virtual T_ operator()(const JointCurveBlock_<T_>& block) const = 0;` and is a
  SIBLING of `Tape::Rate_<T_>` (Phase A untouched). The three projection-capable
  subclasses (`DepositRateProj_<T_>`, `ForwardRateProj_<T_>` covering FRA + Future,
  `SwapRateProj_<T_>`) live in the joint module and are constructed by a
  `ProjectionRateAt<T_>(d, i)` dispatch that reads each instrument's schedule
  directly (NOT through `PrecomputeT`). Pin the exact dispatch and signatures in the
  design doc.
- **OQ-6 (forward-Jacobian capture on the joint diagnostics).** Phase A captures an
  at-solution unscaled forward Jacobian on `CurveCalibrationDiagnostics_::jacobian_`
  (`calibration.hpp:129`) via the solver's convergence-branch hook. Does Phase B add
  an analogous field on `JointCurveCalibrationDiagnostics_` or
  `JointMultiCurveCalibrationResult_`? The current joint diagnostics struct has no
  such field (`jointcalibration.hpp:72-94`). This is a separable diagnostics
  follow-up; out of scope for the core Phase B Jacobian but flagged so it is not
  forgotten.
- **OQ-7 (critique blockers B2 and B3 — RE-DERIVED AND CLOSED under PWL_FWD).** The
  earlier Option-A critique raised two blockers:
  - **B2 (`BuildJointSmoothing` free-knot iteration).** This was a LOG_DISCOUNT
    concern: the single-curve LOG_DISCOUNT path pins knot 0 at today (the anchor)
    and excludes it from the free-parameter vector, so the smoothing metric must be
    rebuilt over the FREE knots (`nKnots - 1`), and `BuildJointSmoothing` (which
    expands knots by `paramsPerKnot` and feeds them to `Underdetermined::SelfCouplePWC`
    at `jointcalibration.cpp:266-280`) would have to be reworked to skip the anchor.
    **Under PWL_FWD this is MOOT:** PWL_FWD has 2 params/knot with NO anchor
    exclusion (`ParamsPerKnot == 2`, every knot free, the parameter vector is
    `2 * nKnots`). `BuildJointSmoothing` already expands every knot by
    `paramsPerKnot == 2` and calls `SelfCouplePWC` over the full
    `2 * nKnots`-length expansion (`jointcalibration.cpp:271-278`). No free-knot
    rework is needed — the smoothing metric is already built over every parameter.
    B2 is CLOSED under Option B.
  - **B3 (anchor audit / `knotDates_.front() > today_` relaxation).** This was a
    LOG_DISCOUNT concern: the single-curve LOG_DISCOUNT anchor is
    `knotDates_.front() == today_` (`calibration.cpp:659-664`), but the joint
    validator REQUIRES `knotDates_.front() > today_` (`jointcalibration.cpp:203-204`),
    so Option A would have required either relaxing the joint validator or adopting
    a `> today_` anchor convention for the templated `DiscountLogDF_<T_>` build.
    **Under PWL_FWD this is MOOT:** PWL_FWD has NO today-pinned anchor at all — the
    parameter vector is `2 * nKnots` with no anchor, the knot abscissae are the knot
    DATES, and knot 0 is a free node like any other. The joint validator's existing
    `> today_` rule (`jointcalibration.cpp:203-204`) stands UNCHANGED; the eligibility
    rule (FR3 (i)) is `tradeDate == knotDates_.front()` (not `tradeDate == today_`).
    No relaxation, no anchor audit, no validator change. B3 is CLOSED under Option B.

  The Option-B risk surface that the re-critique MUST stress is therefore NOT B2/B3
  (both dissolved) but the NEW templated machinery: (1) the forward-to-log-DF
  integration inside `Tape::DiscountPWLF_<T_>` — the PWL `IntegralTo` is a
  piecewise-linear-in-forward accumulation whose `T_`-typed analogue must record on
  the tape correctly across all four backends; (2) the templated base handle
  (`CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<T_>>`) — the first time ANY
  templated curve in the tree has carried a `T_`-typed base (Phase A's
  `DiscountLogDF_<T_>` uses a double base treated as constant); the reverse sweep
  must propagate adjoints through the base multiplication on every backend; (3) the
  2-params/knot column map (no anchor exclusion) — a column-map off-by-one that
  works under LOG_DISCOUNT's `nKnots - 1` would silently misalign under PWL_FWD's
  `2 * nKnots`. These are the load-bearing new-surface concerns.
- **OQ-8 (re-critique blockers B4 and B5 — B4 RESOLVED via CP3; B5 RETIRED).** The
  re-critique (`CONDITIONAL-GO`) flagged two NEW Option-B blockers that changed WHAT
  gets built:
  - **B4 (projection-rate dispatch — RESOLVED via CP3).** The design's
    projection-capable rate classes declared `T_ operator()(const JointCurveBlock_<T_>&
    block)`, but the existing `Tape::Rate_<T_>` virtual is bound to `YCCtx_<T_>` and
    `Swap_::PrecomputeT<T_>()` returns a `Tape::SwapRate_<T_>` reading only
    `ctx.curve_`. So the joint path cannot reuse `PrecomputeT` for the IBOR projection
    slice and cannot broaden `Tape::Rate_<T_>` without breaking Phase A.
    **RESOLUTION: CP3** — a NEW `Tape::JointRate_<T_>` base (sibling, not
    modification) with a `ProjectionRateAt<T_>(d, i)` dispatch that constructs
    projection-capable subclasses directly from instrument schedules. Phase A
    untouched (NG2). See Gap 3, FR5, OQ-5.
  - **B5 (OIS geometric compounding — RETIRED this pass).** The second-pass
    re-critique flagged the first-pass "OISSwap_ rides Swap_" claim as numerically
    unverified and resolved it via CP4 (reject `OISSwap_`). The lead re-verified
    `ycinstrument.cpp` this pass and found the library has NO geometric overnight
    compounding: the AAD path (`ycinstrument.cpp:347-368`) and the double path
    (`ycinstrument.cpp:53-59`) price OIS with the IDENTICAL simple-rate arithmetic,
    and the OIS overnight index has `useProjectionCurve_ == false`
    (`ycinstrument.cpp:44`) so `forecast == discount == OIS`. AAD and bumped evaluate
    the same function on OIS rows (AC1 passes); the arithmetic-vs-geometric gap B5
    worried about does not exist. **RESOLUTION: CP4 RETIRED** — `OISSwap_` is
    ELIGIBLE for ANALYTIC (rides `Swap_::PrecomputeT<T_>`), the example's OIS slice
    stays `OISSwap_`, and `Tape::OisSwapRate_<T_>` is not needed. See Gap 5, FR3 (e),
    OQ-1.

  The S6-S10 completeness items (base-handle ownership via `shared_ptr` `curveStorage`;
  `IntegralTo` four-branch enumeration with `Vector_<T_> sofarT_`; flat `Vector_<T_>`
  members not a templated `PiecewiseLinearT_<T_>`; `DAYS_PER_YEAR` constant + ACT/365F
  eligibility; FR3 (h) defensive/unreachable NOTICE) are folded into Gap 4, Gap 5,
  FR3 (h)/(j), FR5, NG11, and AC11.

## Hand-off

- **Spec path (absolute):**
  `.codex/artifacts/specs/joint-aad-gradient.md`
- **Blocking dependencies:** NONE — Option B is LOCKED; the re-critique's B4 blocker
  is RESOLVED (CP3 new `Tape::JointRate_<T_>` base for the IBOR projection slice);
  B5 is RETIRED (CP4 dropped — `OISSwap_` eligible, rides `Swap_::PrecomputeT<T_>`);
  OQ-7 B2/B3 are RE-DERIVED and CLOSED under PWL_FWD; S6-S10 are folded in. The spec
  is ready for design and implementation.
- **No scope-change flag this pass.** Phase B's ANALYTIC path covers the full vanilla
  instrument set `{Deposit, FRA, Future, Swap}` INCLUDING `OISSwap_`. The second-pass
  CP4 scope reduction (reject `OISSwap_`, switch the example to vanilla `Swap_`,
  defer `Tape::OisSwapRate_<T_>`) is RETIRED — it rested on a geometric-compounding
  concern that the lead verified does not exist in `ycinstrument.cpp` (AAD and double
  paths share identical simple-rate arithmetic; `ycinstrument.cpp:347-368` vs
  `:53-59`). The example's OIS slice stays `OISSwap_` and runs ANALYTIC. No user
  objection is required.
- **Next agent:** `dal-api-designer` to lock the options surface (OQ-4), the
  Jacobian-subclass reuse (OQ-2), and the `Tape::JointRate_<T_>` + `ProjectionRateAt`
  signatures (OQ-5, CP3). Then `dal-critic` for an adversarial pass that STRESSES the
  Option-B risk surface (templated PWL-forward integration, templated base handle
  across four backends, 2-params/knot column map, the new `JointRate_<T_>` dispatch)
  — see OQ-7. Then `dal-implementer`.
- **Suggested branch:** `feature/joint-aad-gradient` off
  `feature/multi-curve-joint-calibration` (NOT off `master` — the joint calibration
  module is not yet merged). Per project memory: do NOT merge; the user merges.

# Joint Multi-Curve AAD Analytic Jacobian (Phase B) - Design

> Status: **REVISED — Option B LOCKED by the user on 2026-06-20.** Parameterization
> is `PIECEWISE_LINEAR_FWD` via a new templated `Tape::DiscountPWLF_<T_>` with a
> templated base handle. Default `jacobianMode_` = `ANALYTIC`, matching single-curve.
> This is DESIGN-ONLY. No implementation. Signatures are illustrative.
> Branch context: `feature/joint-aad-gradient` off
> `feature/multi-curve-joint-calibration` at `b5b8bbf`. Do NOT merge; user merges.
>
> Authoritative inputs (READ before this design):
> - Spec: `.claude/specs/joint-aad-gradient.md` (Option B LOCKED; OQ-1 `OISSwap_`
>   RESOLVED; OQ-7 B2/B3 RE-DERIVED and CLOSED under PWL_FWD).
> - Phase A shipped code: `dal-cpp/dal/curve/calibration.cpp`
>   (`AnalyticJacobian` at `:520-576`, `BuildDiscountCurveT<T_>` at `:239-256`,
>   `PhaseARateAt<T_>` at `:499-510` — dispatches `OISSwap_` via
>   `dynamic_cast<const Swap_*>` at `:507`, `EligibleForAnalyticJacobian` at
>   `:417-438`, `XCurveJacobian_` at `:43-81`, `TapeGuard_` at `:268-280`).
> - Phase B target: `dal-cpp/dal/curve/jointcalibration.cpp`
>   (`JointResidualFunction_::F` at `:311-354`, `JointResidualFunction_::Gradient`
>   at `:357` returning `nullptr`, `ValidateAndBuildSlots` at `:162-260`,
>   `BuildDeclarationCurve` at `:68-95`, `ParamsPerKnot` at `:36-52`,
>   `BuildJointSmoothing` at `:266-280`, `CurveSlot_` at `:144-156`).
> - Double PWL-forward reference: `dal-cpp/dal/curve/ycimp.cpp:56-95`
>   (`DiscountPWLF_` anonymous-namespace class, `operator()` at `:63-66`:
>   `exp(-(fwds_.IntegralTo(to) - fwds_.IntegralTo(from)) / 365.0) * (base_ ? (*base_)(from, to) : 1.0)`,
>   `NewDiscountPWLF` factory at `:85-89`).
> - Options surface: `dal-cpp/dal/curve/jointcalibration.hpp`
>   (`JointCurveDeclaration_` at `:39-51` — DEFAULT `PIECEWISE_LINEAR_FWD` at `:47`,
>   `JointMultiCurveCalibrationSpec_` at `:56-68`, `JointCurveCalibrationDiagnostics_`
>   at `:72-82`, `JointMultiCurveCalibrationResult_` at `:84-94`).
> - AAD facade: `dal-cpp/dal/math/aad/aad.hpp` (all four `#if`/`#elif` blocks:
>   native `:17-54`, Adept `:55-80`, XAD `:81-110`, CoDiPack `:111-134`).
> - Tape machinery: `dal-cpp/dal/curve/ycctx.hpp` (`Tape::YCCtx_<T_>` at `:18-22`,
>   single `curve_` field, no `Forward`), `dal-cpp/dal/curve/discount.hpp`
>   (`Tape::DiscountCurve_<T_>` at `:22-30`), `dal-cpp/dal/curve/yclogdf.hpp`
>   (`Tape::DiscountLogDF_<T_>` at `:25-84`, `CurveWithBase_<DiscountCurve_<T_>,
>   DiscountCurve_<double>>` base at `:26` — UNCHANGED under Option B, which adds a
>   SIBLING class), `dal-cpp/dal/curve/yccomponent.hpp`
>   (`CurveWithBase_<T_, B_ = T_>` at `:24-49`), `dal-cpp/dal/curve/ycinstrument.cpp`
>   (`Tape::DepositRate_<T_>` at `:274`, `Tape::ForwardRate_<T_>` at `:298`,
>   `Tape::SwapRate_<T_>` at `:331-370` with the "forecast == discount == ctx.curve_"
>   comment at `:347-351`, `Swap_::PrecomputeT<T_>()` at `:497` — `OISSwap_` rides it).
> - Underlying Jacobian interface: `dal-cpp/dal/math/optimization/underdetermined.hpp`
>   (`Underdetermined::Jacobian_` at `:46-57`, `Underdetermined::Find` with the
>   `fwd_jacobian_at_solution` out-param at `:74-80`).
> - Double-path routing: `dal-cpp/dal/curve/curveblock.cpp`
>   (`CurveBlock_::Discount` at `:65-74`, `CurveBlock_::Forward` at `:76-83`,
>   the discount-fallback at `:82`), `dal-cpp/dal/curve/ycinstrument.cpp`
>   (`ResolveForecastCurve` at `:41-51`).
> - User memory on portability: `static_cast<double>(Number_)` does NOT compile on
>   XAD/Adept — use `Dal::AAD::Value` everywhere a `Number_` must be read as `double`.
>
> Phase A shipped code is the source of truth for "what works on four backends."
> This design extends it to the joint multi-curve path under Option B. The Phase A
> "Implementation realities" section of `.claude/designs/aad-analytic-jacobian-redesign.md`
> is authoritative on the three specifics it corrects (XAD recording order;
> Adept `ZeroGradientArray`; `static_cast<double>(Number_)` portability).

## 0. The Decision in One Sentence

Ship Phase B as **Option B (PWL_FWD, LOCKED by the user)**: author a NEW templated
`Tape::DiscountPWLF_<T_>` that interpolates forwards on `T_` and integrates forwards
to log-DF with a TEMPLATED BASE HANDLE, add a new `Tape::JointCurveBlock_<T_>` (one
`Number_`-typed curve per collateral and per tenor with `Discount(...)` and
`Forward(...)` reads in the `Number_` domain), author projection-capable `Tape::`
rate subclasses that read both slots, generalize `BuildDeclarationCurve` into a joint
templated builder, and reuse the `Dal::AAD` facade verbatim (no new primitives). The
joint module's native parameterization (`PIECEWISE_LINEAR_FWD`), the joint validator,
and the joint example are all UNCHANGED — Option B AADs the parameterization the joint
module already ships. Default `jacobianMode_` = `ANALYTIC`, matching single-curve.

The four gaps the spec surfaces (multi-curve routing; parameterization fork;
projection-curve fixings; base-handle propagation) are resolved by exactly four
additions: `Tape::DiscountPWLF_<T_>` (the templated PWL-forward curve, new),
`Tape::JointCurveBlock_<T_>`, projection-capable `Tape::Rate_<T_>` subclasses reading
a `JointCurveBlock_<T_>`, and the templated base-layered curve build. None require a
new facade primitive.

## 1. Parameterization Scope Decision (spec Gap 2) — RESOLVED: Option B (LOCKED)

### 1.1 The locked decision and its rationale

**DECISION (user, 2026-06-20): Option B — extend AAD to `PIECEWISE_LINEAR_FWD`.**
The earlier Option A (LOG_DISCOUNT-only) recommendation is SUPERSEDED; the user
LOCKED Option B because it AADs the joint module's native parameterization.

The structural argument for Option B over Option A:

1. **PWL_FWD is the joint module's native parameterization.** The
   `JointCurveDeclaration_` DEFAULT is `PIECEWISE_LINEAR_FWD`
   (`jointcalibration.hpp:47`), `ParamsPerKnot(PIECEWISE_LINEAR_FWD) == 2`
   (`jointcalibration.cpp:36-39`), `BuildDeclarationCurve` builds it
   (`jointcalibration.cpp:74-84`), and the joint example program uses it. Forcing
   every joint declaration onto LOG_DISCOUNT (Option A) would diverge from the module
   default and the example, and would require the joint validator's
   `ParamsPerKnot(LOG_DISCOUNT) == REQUIRE(false)` arm (`jointcalibration.cpp:42-44`)
   to be opened up.

2. **PWL_FWD needs NO anchor relaxation.** Option A's biggest hidden cost was the
   tension between single-curve LOG_DISCOUNT's `knotDates_.front() == today_` anchor
   (`calibration.cpp:659-664`) and the joint validator's
   `knotDates_.front() > today_` rule (`jointcalibration.cpp:203-204`). PWL_FWD has
   NO today-pinned anchor at all — every knot (including knot 0) is a free node, the
   parameter vector is `2 * nKnots`, and the joint validator's `> today_` rule stands
   unchanged. The Option-A-only anchor-vs-validation reconciliation (spec OQ-3 under
   Option A) simply does not arise. (Critique blocker B3 — re-derived and CLOSED in
   spec OQ-7.)

3. **PWL_FWD needs NO smoothing rework.** Option A would have required
   `BuildJointSmoothing` (`jointcalibration.cpp:266-280`) to skip the anchor knot
   when building the smoothing metric over the free knots. PWL_FWD has no anchor to
   skip; `BuildJointSmoothing` already expands every knot by `paramsPerKnot == 2` and
   feeds the full `2 * nKnots` expansion to `Underdetermined::SelfCouplePWC`. No
   rework. (Critique blocker B2 — re-derived and CLOSED in spec OQ-7.)

4. **The cost of Option B is a new templated curve class — accepted.** The new
   `Tape::DiscountPWLF_<T_>` is load-bearing new surface (its own
   forward-integration-on-`T_`, its own templated base handle, its own
   explicit-instantiation gates mirroring the Phase A seam-3 cleanup at
   `yclogdf.cpp`). This is the Option-B risk surface the re-critique must stress
   (spec OQ-7). It is a materially larger PR than Option A would have been, but it
   avoids the parameterization divergence, the validator fork, and the anchor
   reconciliation that Option A would have required.

### 1.2 Why Option A was rejected

The user rejected Option A (LOG_DISCOUNT-only) for three concrete reasons:

- **Divergence from the joint example.** The example program at
  `dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`
  uses the joint module's default `PIECEWISE_LINEAR_FWD`. Option A would have forced
  the example to switch every declaration to `LOG_DISCOUNT` and prepend `today_` to
  the knot ladder — teaching code would then demonstrate a parameterization the
  module does not default to. Option B leaves the example's parameterization alone.
- **Validator fork.** Option A would have required either relaxing the joint
  validator's `knotDates_.front() > today_` rule to `== today_` for LOG_DISCOUNT
  declarations (a parameterization-aware validator branch) or adopting a `> today_`
  anchor convention for the templated `DiscountLogDF_<T_>` build (a new templated
  curve variant). Both are real costs; PWL_FWD avoids both.
- **User preference for the joint example's native PWL_FWD.** The user's stated
  reason for locking Option B is that the joint example's native parameterization is
  the one the AAD path should support. This is a product-level call, not a
  technical-preference call; the design honors it.

### 1.3 What `Tape::DiscountPWLF_<T_>` must provide

The templated PWL-forward curve must, in the `T_` domain:

1. **Store the PWL forward parameters as `T_`.** The double `DiscountPWLF_`
   (`ycimp.cpp:56-83`) holds a `PiecewiseLinear_ fwds_` with `double` `fLeft_` /
   `fRight_` vectors. The templated analogue holds `Vector_<T_> fLeftT_, fRightT_`
   (or a templated `PiecewiseLinearT_<T_>`; the design picks one — see §5.2). The
   knot DATES stay `double`-convertible abscissae (year-fractions or serial-day
   differences); they are NOT `T_`.
2. **Integrate forwards to log-DF on `T_`.** The double body is
   `exp(-(fwds_.IntegralTo(to) - fwds_.IntegralTo(from)) / 365.0) * (base_ ? (*base_)(from, to) : 1.0)`
   (`ycimp.cpp:63-66`). The templated analogue replaces `exp` with `Dal::AAD::exp`,
   the forward integration with a `T_`-typed accumulation (the integral of a
   piecewise-linear forward over a query interval is a sum of trapezoidal areas
   whose weights are `double` knot-abscissa differences and whose values are `T_`
   forward parameters — see §5.3), and the base read with a `T_`-typed
   `(*base_)(from, to)` when a base is supplied.
3. **Carry a TEMPLATED BASE HANDLE.** This is the load-bearing Option-B generalization
   and the thing Phase A never did. Phase A's `Tape::DiscountLogDF_<T_>` inherits
   `CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<double>>` — the base is a
   double-typed curve treated as a CONSTANT from the tape's perspective
   (`calibration.cpp:247-256`). Under Option B, the base-layered forward curve must
   inherit `CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<T_>>` so the base is
   the `T_`-typed discount curve built in the SAME `Gradient` call, and the reverse
   sweep propagates adjoints through the base multiplication into the OIS
   discount-curve free nodes. See §5.

## 2. The Projection-Curve Templating Approach (spec Gap 1 + Gap 3) — RESOLVE

### 2.1 The two structural gaps, restated

Phase A's `Tape::YCCtx_<T_>` (`dal-cpp/dal/curve/ycctx.hpp:18-22`) carries
exactly one `const DiscountCurve_<T_>& curve_` and has no `Forward(...)`. Every
`Tape::Rate_<T_>` subclass reads only `ctx.curve_` — see
`dal-cpp/dal/curve/ycinstrument.cpp:351` ("Phase A eligibility guarantees
forecast == discount == ctx.curve_"). That is correct for Phase A (which
rejects projection-curve instruments at
`dal-cpp/dal/curve/calibration.cpp:467-473`) but is wrong for the joint path:
the joint validator (`dal-cpp/dal/curve/jointcalibration.cpp:225-231`)
**REQUIRES** `useProjectionCurve_ == true` on every forward declaration's
instruments, because a mis-conventioned IBOR leg would fix off the discount
curve and leave the forward curve unconstrained. So the joint AAD path MUST
differentiate both reads.

The double-path mechanism for these two reads is in `CurveBlock_`
(`dal-cpp/dal/curve/curveblock.cpp:65-83`):

- `Discount(collateral)` routes to the discount declaration's curve at
  `collateral`, with an OIS fallback at `:71-73`.
- `Forward(tenor, collateral)` routes to the forward declaration's curve at
  `tenor` if registered, else falls back to `Discount(collateral)` at `:82`.

The joint `JointResidualFunction_::F` assembles a `CurveBlock_`
(`dal-cpp/dal/curve/jointcalibration.cpp:341`) and prices every instrument
through it. The whole curve-build + `CurveBlock_`-assembly + residual-stacking
must be templated on `T_` (spec FR5).

### 2.2 The chosen approach: a templated `Tape::JointCurveBlock_<T_>`

The three shapes the spec offered for Gap 1+3 were: (a) new
`Tape::CurveBlock_<T_>` class, (b) broadened `Tape::YCCtx_<T_>`, (c) pair of
maps passed alongside the context. Resolution: **(a) — a new
`Tape::JointCurveBlock_<T_>`**, *not* a broadening of `Tape::YCCtx_<T_>`.

Why not broaden `Tape::YCCtx_<T_>`:

- `Tape::YCCtx_<T_>` is the Phase A context. Phase A's templated rates
  (`dal-cpp/dal/curve/ycinstrument.cpp:283,311,347`) take a `const
  YCCtx_<T_>& ctx` and read `ctx.curve_`. Broadening `YCCtx_<T_>` to hold
  maps of discount and forward curves would either (i) break Phase A's
  single-curve instantiation, or (ii) leave `curve_` as the discount slot and
  add a forward map that Phase A rates ignore. Both are messy; (i) is a
  regression risk on shipped Phase A code (out of scope per spec NG2).
- A separate `Tape::JointCurveBlock_<T_>` leaves Phase A untouched and makes
  the joint context's multi-curve nature a first-class type. The projection-
  capable `Tape::Rate_<T_>` subclasses (§4) take a
  `const JointCurveBlock_<T_>&` instead of a `const YCCtx_<T_>&` — a distinct
  signature that cannot accidentally be passed a Phase A context.

Why not (c) maps alongside the context: the rates would each have to look up
their collateral and tenor in two maps on every DF read. That is a hot-path
hash lookup on every cash flow period of every swap, recorded once per
`Gradient` call. Routing once at context-build time (into the typed
`JointCurveBlock_<T_>` structure) is cleaner and the rate bodies stay as cheap
as Phase A's.

### 2.3 `Tape::JointCurveBlock_<T_>` shape (illustrative)

Lives in a new header `dal-cpp/dal/curve/jointycctx.hpp` (sibling of
`ycctx.hpp`), under `namespace Dal::Tape`. Holds one `const
DiscountCurve_<T_>&` per collateral and one per tenor — the templated analogue
of the double-path `CurveBlock_`'s two maps. The `Discount`/`Forward` methods
mirror `CurveBlock_::Discount`/`CurveBlock_::Forward`
(`dal-cpp/dal/curve/curveblock.cpp:65-83`) including the OIS fallback and the
forward→discount fallback.

```cpp
// dal-cpp/dal/curve/jointycctx.hpp  (illustrative, NOT a file edit)
namespace Dal { namespace Tape {

    // Phase B templated joint yield-curve context. The multi-curve analogue of
    // YCCtx_<T_>: one Number_-typed discount curve per collateral, one per
    // forward tenor. Routes Discount(collateral) and Forward(tenor, collateral)
    // reads in the T_ domain, mirroring CurveBlock_::Discount/Forward's routing
    // (curveblock.cpp:65-83) including the OIS fallback and the forward->discount
    // fallback. NOT a YieldCurve_ subclass; exists only for the joint AAD-tape
    // residual evaluation in jointcalibration.cpp.
    template <class T_>
    struct JointCurveBlock_ {
        std::map<CollateralType_, const DiscountCurve_<T_>*> discountCurves;
        std::map<PeriodLength_, const DiscountCurve_<T_>*> forwardCurves;

        // Mirror of CurveBlock_::Discount (curveblock.cpp:65-74): exact-collateral
        // match, then OIS fallback. The OIS fallback is the joint post-2008 routing
        // convention and must be preserved on the tape so an OIS knot perturbation
        // flows into an IBOR leg's discounting when collateral != OIS.
        const DiscountCurve_<T_>& Discount(const CollateralType_& c) const;

        // Mirror of CurveBlock_::Forward (curveblock.cpp:76-83): exact-tenor match,
        // else Discount(collateral). The fallback matters: an instrument whose
        // forecast tenor has no registered forward curve routes to the discount
        // curve, and the joint eligibility predicate (FR3) admits this case (a
        // discount declaration instrument with useProjectionCurve_ == false).
        const DiscountCurve_<T_>& Forward(const PeriodLength_& t, const CollateralType_& c) const;
    };
}}  // namespace Dal::Tape
```

The pointer maps (`const DiscountCurve_<T_>*`) are non-owning references to
curves built in the SAME `Gradient` call (see §6). Pointers, not
`Handle_<DiscountCurve_<T_>>`, because the curves live on the stack/unique-ptr
of the `AnalyticJacobian` frame for the duration of the sweep — no shared
ownership needed, and `Handle_` would force a `shared_ptr` round-trip per
curve. The maps are small (1 discount + 1-3 forward declarations) and built
once per `Gradient` call.

### 2.4 Instrument-type coverage (spec FR3 (e), Gap 3, OQ-1 RESOLVED)

The joint AAD path supports exactly the four Phase A instrument types:
`Deposit_`, `FRA_`, `Future_`, `Swap_`. Each maps to a projection-capable
templated rate as follows (see §4 for the rate-subclass design):

| Instrument | Double-path rate (`ycinstrument.cpp`) | Phase B templated rate (new, §4) | Forecast read | Discount read |
|------------|--------------------------------------|----------------------------------|---------------|---------------|
| `Deposit_` | `DepositRate_` (`:73`) | `Tape::DepositRateProj_<T_>` | `Forward(tenor, collateral)` when `useProjectionCurve_`, else `Discount(collateral)` | `Discount(collateral)` |
| `FRA_` | `ForwardRate_` (`:111`) | `Tape::ForwardRateProj_<T_>` | `Forward(tenor, collateral)` | `Discount(collateral)` |
| `Future_` | `ForwardRate_` (`:111`, convexity-adjusted) | `Tape::ForwardRateProj_<T_>` (convexity stays `double`) | `Forward(tenor, collateral)` | `Discount(collateral)` |
| `Swap_` | `SwapRate_` (`:148`) | `Tape::SwapRateProj_<T_>` | `Forward(tenor, collateral)` per float period | `Discount(collateral)` per payment |
| `OISSwap_` | rides `Swap_::Precompute` (inherits `Swap_`) | rides `Swap_::PrecomputeT<T_>()` (`ycinstrument.cpp:497`) | same as `Swap_` (OIS leg compounds overnight off the discount curve) | same as `Swap_` |

**`OISSwap_` is IN SCOPE (spec OQ-1 RESOLVED).** VERIFIED FACT:
`OISSwap_ : public Swap_` (`dal-cpp/dal/curve/ycinstrument.hpp:155`), and
`PhaseARateAt<T_>` dispatches via `dynamic_cast<const Swap_*>(inst)` at
`dal-cpp/dal/curve/calibration.cpp:507` → `s->PrecomputeT<T_>()` at
`ycinstrument.cpp:497`. So `OISSwap_` ALREADY rides the templated `Swap_`
pricing; no new `Tape::OisSwapRate_<T_>` is needed. Critique blocker B1 is
DISSOLVED. The example program's OIS slice (which is `OISSwap_` at
`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp:163-168`)
exercises the joint AAD path natively.

**Explicitly OUT of scope (spec FR3 (e), NG4):** `BasisSwap_` (no Phase A
templated rate; double-path `BasisSwapRate_` resolves TWO forecast curves at
`ycinstrument.cpp:240-251`), `STIR_`. The eligibility predicate (§7 FR3 (e))
rejects any declaration containing an out-of-scope type with a one-time NOTICE
→ bumped.

## 3. No LOG_DISCOUNT Anchor Reconciliation Needed Under Option B

### 3.1 Why this section is short

Under Option A, the design had a full section reconciling single-curve
LOG_DISCOUNT's `knotDates_.front() == today_` anchor with the joint validator's
`knotDates_.front() > today_` rule. **Under Option B this reconciliation does
not arise.** PWL_FWD has NO today-pinned anchor:

- `ParamsPerKnot(PIECEWISE_LINEAR_FWD) == 2` (`jointcalibration.cpp:36-39`), and
  the parameter vector is `2 * nKnots` with EVERY knot free (including knot 0).
  There is no anchor to pin at `today_`.
- The joint validator's `knotDates_.front() > today_` rule
  (`jointcalibration.cpp:203-204`) stands UNCHANGED. No relaxation, no
  parameterization-aware branch.
- `BuildJointSmoothing` (`jointcalibration.cpp:266-280`) already expands every
  knot by `paramsPerKnot == 2` and feeds the full `2 * nKnots` expansion to
  `Underdetermined::SelfCouplePWC`. No free-knot rework.
- The eligibility rule (FR3 (i)) is `tradeDate == knotDates_.front()` (NOT
  `tradeDate == today_`). The example program's instruments already trade at
  knot 0 by construction.

This dissolves critique blockers B2 and B3 (re-derived and CLOSED in spec
OQ-7). The Option-A-only OQ-3 (anchor-vs-validation) is removed from the
spec's open questions under Option B.

### 3.2 What this means for the example program

The example program needs NO parameterization edit under Option B. Its
declarations already inherit `PIECEWISE_LINEAR_FWD` from the
`JointCurveDeclaration_` default (`jointcalibration.hpp:47`), its knot ladder
already starts strictly after `today_` (satisfying the validator), and its OIS
slice is `OISSwap_` (which rides `Swap_::PrecomputeT<T_>()`, §2.4). The only
example edit Phase B requires is requesting `ANALYTIC` via the new two-arg
overload — and under the LOCKED default (`ANALYTIC`), even that is optional
(see §9.2).

## 4. Projection-Capable Templated Rates (spec Gap 3, OQ-5) — RESOLVE

### 4.1 The chosen design: new projection-capable `Tape::Rate_<T_>` subclasses

The spec offered three options for Gap 3: (i) new `Tape::` projection-capable
rate subclasses, (ii) broadened existing `Tape::Rate_<T_>` subclasses that
accept a templated forecast source, (iii) templated
`ResolveForecastCurve`/`ForwardRate` pair reading off a templated forecast
source. Resolution: **(i) — new projection-capable subclasses**, taking a
`const JointCurveBlock_<T_>&` (not a `const YCCtx_<T_>&`).

Why not (ii) broaden the existing subclasses:

- The existing `Tape::DepositRate_<T_>`, `Tape::ForwardRate_<T_>`,
  `Tape::SwapRate_<T_>` (`dal-cpp/dal/curve/ycinstrument.cpp:274,298,331`)
  are Phase A's. Phase A instantiates them with a `YCCtx_<T_>` (single curve).
  Broadening them to take "either a `YCCtx_<T_>` or a `JointCurveBlock_<T_>`"
  requires either a template-template parameter, a variant, or a concept —
  all of which couple Phase A's rate signatures to the joint context's
  existence. Phase A is shipped (NG2); this coupling is a regression risk on
  code that is out of scope.
- The "forecast == discount == ctx.curve_" assumption at
  `dal-cpp/dal/curve/ycinstrument.cpp:347-351` is *load-bearing* for Phase A's
  eligibility. Broadening the rate to optionally read a separate forecast
  source would make that assumption conditional, and the Phase A eligibility
  predicate (`dal-cpp/dal/curve/calibration.cpp:467-473`) would have to be
  tightened to re-assert it. Net: more risk, no benefit.

Why not (iii) templated `ResolveForecastCurve`/`ForwardRate`: the existing
`Tape::ForwardRate` free function at
`dal-cpp/dal/curve/ycinstrument.cpp:264-272` already takes a
`const DiscountCurve_<T_>& forecast` — it IS templated and reusable as-is.
The gap is not in `ForwardRate`; it is in the rate *classes* that decide WHICH
curve to read (discount vs forecast). That decision lives in the rate
subclass's `operator()`, which currently hard-codes `ctx.curve_`. So the gap
is precisely the rate subclasses, which is what (i) addresses.

### 4.2 The projection-capable rate shape (illustrative)

Each projection-capable rate lives in an anonymous namespace in
`dal-cpp/dal/curve/jointcalibration.cpp` (mirroring Phase A's anonymous-
namespace templated rates in `ycinstrument.cpp:256-371`). The arithmetic
bodies are byte-identical to the double-path rate bodies in
`dal-cpp/dal/curve/ycinstrument.cpp:73-251`, with `T_` in place of `double`
for the DF reads and the rate/annuity/fixing accumulations; dcf, yf, fixing
dates, convexity adjustments stay `double` (schedule-driven constants).

The `Forward(tenor, collateral)` read happens once per fixing (or per float
period for swaps); the `Discount(collateral)` read happens once per payment
date. Both reads return `T_` and the tape records the dependence on every
curve's parameters.

```cpp
// dal-cpp/dal/curve/jointcalibration.cpp  (illustrative, anonymous namespace)
namespace Dal { namespace Tape {

    // Resolve a Number_-typed forecast curve: the forecast tenor's registered
    // forward curve when useProjectionCurve_, else the discount curve at the
    // convention's collateral. Mirrors ResolveForecastCurve (ycinstrument.cpp:41-51)
    // and CurveBlock_::Forward's fallback (curveblock.cpp:76-83).
    template <class T_>
    const DiscountCurve_<T_>& ResolveForecast(const JointCurveBlock_<T_>& block,
                                               const RateIndexConvention_& conv) {
        return conv.useProjectionCurve_
            ? block.Forward(conv.forecastTenor_, conv.collateral_)
            : block.Discount(conv.collateral_);
    }

    template <class T_>
    class DepositRateProj_ : public Rate_<T_> {
        Date_ start_, maturity_;
        RateIndexConvention_ convention_;
    public:
        DepositRateProj_(const Date_& start, const Date_& maturity, const RateIndexConvention_& conv)
            : start_(start), maturity_(maturity), convention_(conv) {}

        T_ operator()(const JointCurveBlock_<T_>& block) const {
            // forecast read off ResolveForecast, discount off Discount(collateral).
            // For a deposit the fixing and the payment coincide, so the discount
            // curve is read once at the maturity; the forecast curve is read for
            // the fixing. Both are T_-typed; the tape records both dependencies.
            const DiscountCurve_<T_>& forecast = ResolveForecast<T_>(block, convention_);
            SchedulePeriod_ period = /* same body as Tape::DepositRate_<T_>::operator() */;
            return ForwardRate(forecast, period.accrualStart_, period.accrualEnd_,
                               convention_.dayBasis_, period.dayCountContext_);
        }
    };

    template <class T_>
    class SwapRateProj_ : public Rate_<T_> {
        Date_ tradeDate_;
        Vector_<CouponPeriod_> fixedPeriods_, floatPeriods_;
        RateIndexConvention_ floatIndexConvention_;
    public:
        SwapRateProj_(/* same ctor args as Tape::SwapRate_<T_> */);

        T_ operator()(const JointCurveBlock_<T_>& block) const {
            // Discount and forecast are SEPARATE curves in the Number_ domain:
            // annuity and every payment's DF read off Discount(collateral);
            // every float period's fixing reads off Forward(tenor, collateral).
            const DiscountCurve_<T_>& discount = block.Discount(floatIndexConvention_.collateral_);
            const DiscountCurve_<T_>& forecast = ResolveForecast<T_>(block, floatIndexConvention_);
            T_ annuity(/* 0.0 */);
            for (const auto& period : fixedPeriods_)
                annuity += static_cast<double>(period.accrual_.dcf_) * discount(tradeDate_, period.schedule_.paymentDate_);
            REQUIRE(Dal::AAD::Value(annuity) > 0.0, "Swap pricing requires positive fixed-leg annuity");
            T_ floatPv(/* 0.0 */);
            for (const auto& period : floatPeriods_) {
                const T_ fixing = ForwardRate(forecast, period.schedule_.accrualStart_,
                                              period.schedule_.accrualEnd_,
                                              floatIndexConvention_.dayBasis_,
                                              period.schedule_.dayCountContext_);
                floatPv += fixing * static_cast<double>(period.accrual_.dcf_)
                         * discount(tradeDate_, period.schedule_.paymentDate_);
            }
            return floatPv / annuity;
        }
    };
}}  // namespace Dal::Tape
```

`ForwardRateProj_<T_>` (FRA and Future) follows the same pattern. Future's
convexity adjustment stays `double` (a schedule-driven constant), subtracted
after the forecast read — identical to the existing
`Tape::ForwardRate_<T_>` at `dal-cpp/dal/curve/ycinstrument.cpp:311-329`.

The `OISSwap_` case rides `SwapRateProj_<T_>` (or the dispatch at §6.2 step 5
calls `s->PrecomputeT<T_>()` and adapts it to the `JointCurveBlock_<T_>` shape
— see OQ-5 for the precise dispatch design). Either way, no
`Tape::OisSwapRate_<T_>` is authored; the existing `Swap_` templated rate is
reused.

## 5. `Tape::DiscountPWLF_<T_>` and Base-Handle Propagation (spec Gap 2 + Gap 4) — RESOLVE

### 5.1 The requirement (two parts)

**Part A — the templated PWL-forward curve.** The new `Tape::DiscountPWLF_<T_>`
must reproduce the double `DiscountPWLF_::operator()` arithmetic at
`ycimp.cpp:63-66` in the `T_` domain:

```
operator()(from, to) = Dal::AAD::exp(-(IntegralTo(to) - IntegralTo(from)) / 365.0)
                       * (base_ ? (*base_)(from, to) : T_(1.0))
```

where `IntegralTo(t)` is the `T_`-typed integral of the piecewise-linear
forward from the first knot to `t`. The `365.0` denominator and the knot
abscissae are `double` (schedule-driven constants); the forward parameters
`fLeftT_[k]`, `fRightT_[k]` are `T_` and registered as independents on the
tape (§6.2 step 1).

**Part B — the templated base handle.** When the declaration is
base-layered, the curve must multiply by a `T_`-typed base read
`(*base_)(from, to)` where `base_` is the discount declaration's
`Tape::DiscountPWLF_<T_>` (or, more precisely, a `Tape::DiscountCurve_<T_>*`
pointing at it) built in the SAME `Gradient` call. The reverse sweep must
propagate adjoints through that multiplication into the OIS discount-curve
free nodes. This is the load-bearing Option-B generalization: Phase A's
`Tape::DiscountLogDF_<T_>` uses a `DiscountCurve_<double>` base treated as a
constant; Option B's `Tape::DiscountPWLF_<T_>` uses a `DiscountCurve_<T_>`
base whose adjoints propagate.

### 5.2 Class shape (illustrative)

Lives in a new header `dal-cpp/dal/curve/ycpwlf.hpp` (sibling of `yclogdf.hpp`),
under `namespace Dal::Tape`. The base type is a SECOND template parameter
(defaulted to `DiscountCurve_<double>` for the baseless / constant-base case,
matching Phase A's pattern):

```cpp
// dal-cpp/dal/curve/ycpwlf.hpp  (illustrative, NOT a file edit)
namespace Dal { namespace Tape {

    // Phase B templated PWL-forward curve. Interpolates forwards piecewise-linearly
    // on T_ (fLeftT_/fRightT_ are the 2 * nKnots free parameters), integrates
    // forwards to log-DF on T_, and multiplies by a T_-typed base when supplied.
    // The double specialization (T_ = double) is byte-for-byte identical to the
    // anonymous-namespace DiscountPWLF_ at ycimp.cpp:56-83. The Number_
    // specialization is constructed only by the AAD-tape Gradient override in
    // jointcalibration.cpp. Construction is factory-only via NewDiscountPWLFT<>
    // (double) -- the Number_ factory lives in jointcalibration.cpp and is
    // unreachable from public code.
    template <class T_, class B_ = DiscountCurve_<double>>
    class DiscountPWLF_ : public CurveWithBase_<DiscountCurve_<T_>, B_>, public FittableCurve_ {
        Vector_<Date_> knotDates_;
        // T_-typed PWL forward parameters: fLeftT_[k], fRightT_[k] per knot k.
        // Registered as independents on the tape (the free-parameter vector is
        // 2 * nKnots with NO anchor exclusion -- every knot is free).
        Vector_<T_> fLeftT_, fRightT_;
        // Double knot abscissae (serial-day differences or year-fractions). Computed
        // once at construction; identical for any T_.
        Vector_<double> knotAbscissae_;

    public:
        DiscountPWLF_(const String_& name,
                      const String_& ccy,
                      const Vector_<Date_>& knotDates,
                      const Vector_<T_>& fLeftT,
                      const Vector_<T_>& fRightT,
                      const Handle_<DiscountCurve_<B_>>& base = Handle_<DiscountCurve_<B_>>());

        // The arithmetic body mirrors ycimp.cpp:63-66:
        //   exp(-(IntegralTo(to) - IntegralTo(from)) / 365.0) * (base_ ? base_(from,to) : 1.0)
        // with Dal::AAD::exp for the T_ path and std::exp for the double path
        // (dispatch via if constexpr, mirroring Tape::DiscountLogDF_<T_>::LogDfAt).
        T_ operator()(const Date_& from, const Date_& to) const override;

        // T_-typed forward integral from knot 0 to serial-day abscissa t. The
        // integral of a piecewise-linear forward over [t_a, t_b] is a sum of
        // trapezoidal areas: sum over segments of (fL + fR)/2 * (abscissa diff),
        // where the abscissa diffs are double constants and the fL/fR are T_.
        [[nodiscard]] T_ IntegralTo(double t) const;

        [[nodiscard]] int NX() const override;
        void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] DiscountPWLF_<T_, B_>* Clone(const String_& new_name,
                                                    const YCComponent_::substitutions_t& base_changes) const override;

        [[nodiscard]] const Vector_<Date_>& KnotDates() const { return knotDates_; }
        [[nodiscard]] Vector_<T_> FLeft() const;
        [[nodiscard]] Vector_<T_> FRight() const;
    };
}}  // namespace Dal::Tape

// The double alias preserves the public name for the double path.
// NOTE: the existing anonymous-namespace DiscountPWLF_ at ycimp.cpp:56-83 is NOT
// removed in Phase B (it is the storable/serializable double curve). The templated
// Tape::DiscountPWLF_<double> is byte-for-byte identical in arithmetic; whether the
// double path switches to the templated class is a cleanup decision for
// dal-api-designer (out of scope for Phase B's core Jacobian -- flagged OQ-A).
```

### 5.3 The `T_`-typed forward integral (the load-bearing new arithmetic)

The double `PiecewiseLinear_::IntegralTo` integrates a piecewise-linear function
as a sum of trapezoidal areas. The templated analogue must do the same on `T_`:

```
IntegralTo(t) = sum over knots k where knotAbscissae_[k] <= t of
                  integral of the PWL forward over [knotAbscissae_[k-1], knotAbscissae_[k]]
              + (partial trapezoid on the segment containing t)
```

For a full segment `[k-1, k]` (abscissa diff `dt = knotAbscissae_[k] - knotAbscissae_[k-1]`,
both `double`):

```
segment_integral = (fLeftT_[k-1] + fRightT_[k-1]) / 2.0 * dt
                 + (fRightT_[k-1] + fLeftT_[k]) / 2.0 * 0   // discontinuity handled per the double PiecewiseLinear_ convention
```

(The exact trapezoidal decomposition matches the double `PiecewiseLinear_` at
`dal-cpp/dal/curve/piecewiselinear.hpp` — the implementer reads it before
coding; this design does NOT re-derive it, only pins that the `T_`-typed
analogue uses `double` abscissa weights and `T_` forward values.) The
abscissa weights are `double` constants computed once at construction; the
forward values are `T_` and the tape records the dependence on every
`fLeftT_[k]` / `fRightT_[k]`. The `/ 365.0` and the `Dal::AAD::exp` are
applied once per `operator()` call, not per segment.

### 5.4 The templated base handle — why this is new

Phase A's `Tape::DiscountLogDF_<T_>` inherits
`CurveWithBase_<DiscountCurve_<T_>, DiscountCurve_<double>>` — the base is
double-typed and treated as a CONSTANT from the tape's perspective
(`calibration.cpp:247-256`). That is correct for Phase A (no base layering;
the single base curve is exogenous).

Under Option B, the base-layered forward declaration's
`Tape::DiscountPWLF_<Number_, DiscountCurve_<Number_>>` must hold a
`Number_`-typed base (the discount declaration's curve built in the same
sweep), so its adjoints propagate. The generalization is the second template
parameter: `B_ = DiscountCurve_<double>` for the baseless / constant-base
case (Phase A's pattern, byte-for-byte), `B_ = DiscountCurve_<T_>` for the
base-layered case (Option B's new pattern). The `operator()` body multiplies
by `(*base_)(from, to)`; with a `T_`-typed base that multiplication records
on the tape and the reverse sweep propagates into the base curve's parameters.

### 5.5 The build order that makes the base resolve

The templated joint residual build preserves the double path's two-pass order
(`dal-cpp/dal/curve/jointcalibration.cpp:317-340`): pass 1 builds every
discount declaration's curve as `Tape::DiscountPWLF_<Number_>` (baseless);
pass 2 builds every forward declaration's curve as
`Tape::DiscountPWLF_<Number_, DiscountCurve_<Number_>>` with
`base = &discountDecls[targetCollateral_]` when `baseLayeredOverDiscount_`,
else as `Tape::DiscountPWLF_<Number_>` (baseless).

The `JointCurveBlock_<Number_>` is assembled AFTER both passes (so every
curve pointer is live), then the residual loop reads through it. The base
reference inside the forward declaration's curve points at a curve on the
`AnalyticJacobian` frame's stack/unique-ptrs; the `JointCurveBlock_<Number_>`
holds non-owning pointers to the same curves. Both lifetimes end with the
`Gradient` call.

### 5.6 How OIS adjoints reach the forward curve

When the reverse sweep seeds an IBOR-swap residual row, the adjoint propagates
backward through the swap's `operator()`:

1. Through the discount read at every payment date → into the discount
   declaration's free-node forward parameters (`fLeftT_`/`fRightT_`). This is
   the IBOR-discounting channel; it exists whether or not base layering is on.
2. Through the forecast read at every fixing → into the forward declaration's
   free-node forward parameters. This is the forecast channel; it is the whole
   point of Gap 3.
3. **Through the forward curve's `operator()` → through the `exp(-integral/365)`
   term → into the forward declaration's free-node forward parameters, AND —
   when `baseLayeredOverDiscount_` — through the `(*base_)(from, to)`
   multiplication → into the discount declaration's free-node forward
   parameters.** This is the base-handle channel; it is the whole point of
   Gap 4. It exists ONLY when `baseLayeredOverDiscount_` is true, because only
   then is the forward declaration's base a `T_`-typed curve rather than a
   double constant.

AC5 in the spec (base-handle propagation) asserts channel 3 produces a
non-zero OIS-knot sensitivity in the forward declaration's residual rows,
matching FD. Channel 3 is the test that proves the
`CurveWithBase_<DiscountCurve_<Number_>, DiscountCurve_<Number_>>`
generalization in `Tape::DiscountPWLF_<T_, B_>` is wired correctly.

## 6. The Templated Joint Residual and the AAD Cycle (spec FR5, FR2) — RESOLVE

### 6.1 The recording contract (HARD CONSTRAINT — works on all four backends)

The recording order is authoritative per Phase A's "Implementation realities"
item 1 (`.claude/designs/aad-analytic-jacobian-redesign.md`) and user memory
on AAD backend recording contracts. The order that works on all four backends
is:

```
Clear(tape)                -- TapeGuard_ ctor
RegisterIndependent x N    -- every free parameter, every declaration.
                             PWL_FWD: 2 * nKnots per declaration (NO anchor
                             exclusion -- every knot is free).
NewRecording(tape)         -- opened AFTER registration, BEFORE the forward pass
forward pass               -- build every Number_-typed Tape::DiscountPWLF_<Number_>
                             (baseless or base-layered), assemble
                             JointCurveBlock_<Number_>, compute stacked residuals
for each row i:
    ZeroAdjoints(tape)
    Adjoint(residuals[i]) = 1.0
    PropagateToStart(tape)
    harvest j(i, col) = Adjoint(fLeftT_or_fRightT[col]) for every declaration's
                        free parameters
```

This is the same order Phase A uses
(`dal-cpp/dal/curve/calibration.cpp:520-575`). The differences under Option B:
(i) the independent-registration step registers EVERY declaration's `2 * nKnots`
parameters (no anchor exclusion — the critical structural difference from
LOG_DISCOUNT's `nKnots - 1`); (ii) the forward pass builds
`Tape::DiscountPWLF_<Number_>` instead of `Tape::DiscountLogDF_<Number_>`; (iii)
the harvest step reads adjoints across every declaration's `2 * nKnots`
parameters via the column map `solver col (slot.paramOffset + j) = declaration d's
storage parameter j`.

**`Dal::AAD::Value` is used to extract doubles from `Number_`** (NEVER
`static_cast<double>(Number_)`, which does not compile on XAD/Adept — user
memory on `static_cast<double>(Number_)` portability). The one
`static_cast<double>` in the body is on `slot.marketRates[i]` (a `double`, not
a `Number_`) — legal and intended, matching Phase A at
`dal-cpp/dal/curve/calibration.cpp:555`.

### 6.2 The `AnalyticJacobian` body sketch (illustrative)

Lives in `dal-cpp/dal/curve/jointcalibration.cpp`'s `JointResidualFunction_`
(or a free helper it calls), mirroring Phase A's
`YieldCurveCalibrationFunc_::AnalyticJacobian`
(`dal-cpp/dal/curve/calibration.cpp:520-576`). The `TapeGuard_` is identical
(Phase A's at `:268-280` is reused verbatim — Clear-only ctor/dtor, exception-
safe).

```cpp
// dal-cpp/dal/curve/jointcalibration.cpp  (illustrative, in JointResidualFunction_)
Underdetermined::Jacobian_* JointResidualFunction_::AnalyticJacobian(
    const Vector_<>& x, const Vector_<>& /*f*/) const {

    auto* tape = Dal::AAD::Tape();
    TapeGuard_ guard(tape);   // Clear on entry, Clear on exit (Phase A verbatim)

    // 1. Register EVERY declaration's 2 * nKnots free parameters as independents.
    //    PWL_FWD has NO anchor exclusion -- every knot (including knot 0) is free.
    //    The column map is per-declaration: solver column (slot.paramOffset + j)
    //    maps to declaration d's storage parameter j directly (j in [0, 2*nKnots)).
    //    The PWL storage interleaves fLeft[k], fRight[k] at indices 2k, 2k+1,
    //    matching BuildDeclarationCurve at jointcalibration.cpp:79-82.
    std::vector<std::pair<Vector_<Dal::AAD::Number_>, Vector_<Dal::AAD::Number_>>>>
        fwdParamsPerDecl(slots_->size());
    for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
        const CurveSlot_& slot = (*slots_)[d];
        const int nKnots = static_cast<int>(slot.knotDates.size());
        auto& [fLeftT, fRightT] = fwdParamsPerDecl[d];
        fLeftT.resize(nKnots);
        fRightT.resize(nKnots);
        for (int k = 0; k < nKnots; ++k) {
            Dal::AAD::RegisterIndependent(fLeftT[k],  x[slot.paramOffset + 2 * k]);
            Dal::AAD::RegisterIndependent(fRightT[k], x[slot.paramOffset + 2 * k + 1]);
        }
    }

    // 2. Open the recording AFTER registering the independents (XAD contract).
    Dal::AAD::NewRecording(*tape);

    // 3. Two-pass templated curve build. Pass 1: discount declarations as
    //    Tape::DiscountPWLF_<Number_> (baseless). Pass 2: forward declarations,
    //    with base = the Number_-typed discount curve at targetCollateral_ when
    //    baseLayeredOverDiscount_ (Gap 4 wiring).
    std::map<CollateralType_, const Tape::DiscountCurve_<Dal::AAD::Number_>*> discountT;
    std::map<PeriodLength_, const Tape::DiscountCurve_<Dal::AAD::Number_>*> forwardT;
    std::vector<std::unique_ptr<Tape::DiscountCurve_<Dal::AAD::Number_>>> curveStorage;
    curveStorage.reserve(slots_->size());

    // Pass 1
    for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
        const CurveSlot_& slot = (*slots_)[d];
        const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
        if (!decl.calibrateDiscountCurve_)
            continue;
        const auto& [fLeftT, fRightT] = fwdParamsPerDecl[d];
        curveStorage.push_back(
            BuildDeclarationCurveT<Dal::AAD::Number_>(decl, ccy_, slot.knotDates, fLeftT, fRightT));
        discountT[decl.targetCollateral_] = curveStorage.back().get();
    }
    // Pass 2
    for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
        const CurveSlot_& slot = (*slots_)[d];
        const JointCurveDeclaration_& decl = spec_->curves_[slot.curveIndex];
        if (decl.calibrateDiscountCurve_)
            continue;
        const auto& [fLeftT, fRightT] = fwdParamsPerDecl[d];
        const Tape::DiscountCurve_<Dal::AAD::Number_>* base = nullptr;
        if (decl.baseLayeredOverDiscount_)
            base = discountT.at(decl.targetCollateral_);   // Gap 4: Number_-typed base
        curveStorage.push_back(
            BuildDeclarationCurveT<Dal::AAD::Number_>(decl, ccy_, slot.knotDates, fLeftT, fRightT, base));
        forwardT[decl.targetTenor_] = curveStorage.back().get();
    }

    // 4. Assemble the Number_-typed joint routing context (Gap 1 wiring).
    Tape::JointCurveBlock_<Dal::AAD::Number_> block;
    block.discountCurves = discountT;
    block.forwardCurves = forwardT;

    // 5. Compute Number_-typed stacked residuals via the projection-capable rates.
    //    OISSwap_ rides Swap_::PrecomputeT<Number_>() (spec OQ-1 RESOLVED) -- the
    //    dispatch is the joint analogue of PhaseARateAt<T_> at calibration.cpp:499.
    int totalResiduals = 0;
    for (const auto& slot : *slots_) totalResiduals += slot.nInstruments;
    Vector_<Dal::AAD::Number_> residuals(totalResiduals);
    int offset = 0;
    for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
        const CurveSlot_& slot = (*slots_)[d];
        for (int i = 0; i < slot.nInstruments; ++i) {
            Handle_<Tape::Rate_<Dal::AAD::Number_>> rateT =
                ProjectionRateAt<Dal::AAD::Number_>(d, i);   // §4 dispatch
            residuals[offset + i] = (*rateT)(block) - static_cast<double>(slot.marketRates[i]);
        }
        offset += slot.nInstruments;
    }

    // 6. Dense Jacobian (totalResiduals x totalFreeParams), single-result reverse sweep.
    const int nCols = static_cast<int>(x.size());
    Matrix_<> j(totalResiduals, nCols, 0.0);
    for (int i = 0; i < totalResiduals; ++i) {
        Dal::AAD::ZeroAdjoints(*tape);
        Dal::AAD::Adjoint(residuals[i]) = 1.0;
        Dal::AAD::PropagateToStart(*tape);
        // Harvest per-declaration: declaration d's column (slot.paramOffset + 2k)
        // reads Adjoint(fLeftT[k]) and column (slot.paramOffset + 2k+1) reads
        // Adjoint(fRightT[k]). PWL_FWD has NO anchor exclusion -- k runs [0, nKnots).
        for (int d = 0; d < static_cast<int>(slots_->size()); ++d) {
            const CurveSlot_& slot = (*slots_)[d];
            const int nKnots = static_cast<int>(slot.knotDates.size());
            const auto& [fLeftT, fRightT] = fwdParamsPerDecl[d];
            for (int k = 0; k < nKnots; ++k) {
                j(i, slot.paramOffset + 2 * k)     = Dal::AAD::Value(Dal::AAD::Adjoint(fLeftT[k]));
                j(i, slot.paramOffset + 2 * k + 1) = Dal::AAD::Value(Dal::AAD::Adjoint(fRightT[k]));
            }
        }
    }
    return new XCurveJacobian_(std::move(j));   // §8 OQ-2 resolution: shared header
}
```

`static_cast<double>(slot.marketRates[i])` at the residual line is on a
`double` (the market rate), NOT a `Number_` — this matches Phase A at
`dal-cpp/dal/curve/calibration.cpp:555` and is the one `static_cast<double>`
in the body. The market rate is a constant, contributes zero to the Jacobian,
and is intentionally NOT a `Number_`.

`BuildDeclarationCurveT<T_>` is the joint analogue of Phase A's
`BuildDiscountCurveT<T_>` (`dal-cpp/dal/curve/calibration.cpp:239-256`). Under
Option B it REQUIREs `PIECEWISE_LINEAR_FWD`, constructs
`Tape::DiscountPWLF_<T_>` for the baseless case, and
`Tape::DiscountPWLF_<T_, DiscountCurve_<T_>>` for the base-layered case
(§5.4). The template specialization for `T_ = Number_` is the only one
exercised; the `T_ = double` specialization is not needed on the AAD path
(the double path stays in `BuildDeclarationCurve` at
`dal-cpp/dal/curve/jointcalibration.cpp:68-95`).

### 6.3 TapeGuard_ reuse

Phase A's `TapeGuard_` (`dal-cpp/dal/curve/calibration.cpp:268-280`) is
identical to what the joint path needs (Clear-only ctor/dtor, exception-safe,
single-threaded contract). Two options:

- **(a)** Port it verbatim into `jointcalibration.cpp`'s anonymous namespace
  (small struct, ~13 lines).
- **(b)** Factor it into a shared header (e.g.
  `dal-cpp/dal/curve/tapeguard.hpp`) and have both TUs include it.

Resolution: **(a)** for Phase B. The struct is trivial and the joint path is
the only second consumer; factoring into a header for two consumers is
premature. If a third consumer appears, factor then. (See §8 OQ-2 for the
parallel decision on `XCurveJacobian_`, where the answer is different because
the Jacobian subclass is non-trivial.)

## 7. Eligibility Contract (spec FR3) — RESOLVE

### 7.1 The joint analogue of `EligibleForAnalyticJacobian`

Phase A's `EligibleForAnalyticJacobian`
(`dal-cpp/dal/curve/calibration.cpp:417-438`) is a per-spec predicate:
LOG_DISCOUNT + DISCOUNT-target + every instrument has a templated rate + no
projection curve + tradeDate == anchor. The joint analogue is a per-SPEC
predicate (one verdict for the whole joint system, not per-declaration),
because the joint AAD path is all-or-nothing: a single ineligible declaration
disables the joint AAD path (mirroring Phase A's "no mixed-row mode" at
`.claude/designs/aad-analytic-jacobian-selector-api.md:71-77`).

The verdict is computed once and cached (`Eligibility_{Unknown, Eligible,
Ineligible}` member on `JointResidualFunction_`, evaluated lazily on first
`Gradient` or eagerly in the ctor), mirroring Phase A's cache at
`dal-cpp/dal/curve/calibration.cpp:282-307, 396-409`. Every NOTICE fires at
most once per `CalibrateJointMultiCurve` call (Phase A H1 contract,
`.claude/designs/aad-analytic-jacobian-naming-and-flag.md:506-527`).

### 7.2 The joint eligibility clauses (FR3 (a)-(i), resolved per Option B)

| Clause | Phase A analogue | Joint resolution under Option B |
|--------|------------------|---------------------------------|
| (a) `jacobianMode_ == ANALYTIC` | `:373` | Same — the runtime flag is the entry gate. Default is ANALYTIC under Option B (§9.2). |
| (b) native build OR external verified | compile-time gate (DROPPED in Phase A ship per Decision 6 of `redesign.md`) | NO compile-time gate (matching shipped Phase A). The four-backend test mandate (AC6) is the verification. |
| (c) every declaration's parameterization is AAD-eligible | `parameterization_ == LOG_DISCOUNT` (`:418`) | Every declaration's `parameterization_ == PIECEWISE_LINEAR_FWD` (Option B). Non-PWL_FWD declaration → NOTICE naming the declaration index and the offending parameterization → Ineligible. (LOG_DISCOUNT remains AAD-eligible on the single-curve path; it is out of scope for the JOINT AAD path under Option B's first cut — spec NG9.) |
| (d) solveMode consistency | (Phase A captures fwd J only under EXACT at `:824`) | ANALYTIC engages for both EXACT and APPROXIMATE (the Jacobian is well-defined in both); the at-solution forward-Jacobian capture (§9) is EXACT-only, matching Phase A. |
| (e) instrument type in `{Deposit, FRA, Future, Swap}` (OISSwap_ rides Swap_) | `:434-437` walks every instrument | Every instrument in every declaration is one of the four (with `OISSwap_` riding `Swap_::PrecomputeT<T_>()` — spec OQ-1 RESOLVED, critique blocker B1 DISSOLVED). `BasisSwap_`, `STIR_` → NOTICE naming the declaration index, instrument name, and type → Ineligible. |
| (f) forward-declaration instruments project | (Phase A rejects projection at `:467-473`) | Every forward-declaration instrument has `useProjectionCurve_ == true` — structurally guaranteed by `ValidateAndBuildSlots` (`jointcalibration.cpp:225-231`), but the eligibility predicate names it in the NOTICE for clarity. |
| (g) discount/baseless-forward instruments don't project | (Phase A: `:467-473`) | Every discount-declaration instrument AND every baseless-forward-declaration instrument has `useProjectionCurve_ == false`. A discount-declaration instrument that projects would route off a not-yet-built forward curve. |
| (h) base-layered forward's base collateral is an eligible discount declaration | (no Phase A analogue — Phase A has no base layering) | Every base-layered forward declaration's `targetCollateral_` is produced by a discount declaration in the same spec whose parameterization is also PWL_FWD (so the base is a `T_`-typed curve, not a double constant). `ValidateAndBuildSlots` already guarantees the collateral is produced AND that base layering is only on PWL_FWD (`jointcalibration.cpp:213-223`); clause (c) already guarantees the discount declaration is PWL_FWD; so (h) is structurally guaranteed once (c) holds, but the predicate names it in the NOTICE. |
| (i) tradeDate == knot 0 (NOT today_) | `:479-485` | Every instrument's `TradeDate()` equals its declaration's knot 0 (`knotDates_.front()`). Under PWL_FWD the anchor is knot 0, which the validator REQUIRES to be `> today_` (`jointcalibration.cpp:203-204`) — so the rule is `tradeDate == knotDates_.front()`, NOT `tradeDate == today_`. This dissolves the LOG_DISCOUNT-only anchor-vs-validation tension (critique blocker B3, re-derived and CLOSED in spec OQ-7). |

### 7.3 NOTICE text (illustrative, mirroring Phase A's at
`dal-cpp/dal/curve/calibration.cpp:419-484`)

Every fall-through emits a NOTICE naming the declaration index (and instrument
name where applicable) and the failing condition. Examples:

- `"Joint AAD Jacobian requires CurveParameterization_::PIECEWISE_LINEAR_FWD on every declaration; declaration N has X; falling back to bumped"`
- `"Joint AAD Jacobian has no templated rate for instrument 'name' (type) in declaration N; falling back to bumped"`
- `"Joint AAD Jacobian requires every instrument to trade at its declaration's knot 0; instrument 'name' in declaration N does not, falling back to bumped"`
- `"Joint AAD Jacobian requires base-layered forward declaration N's base collateral to be an eligible discount declaration; falling back to bumped"` (defensive — structurally guaranteed)

The text uses "Joint AAD Jacobian" (not "ANALYTIC Jacobian") to distinguish
joint NOTICEs from single-curve NOTICEs in a quant's log. Cosmetic; reviewer's
call.

### 7.4 FR6 — ANALYTIC never throws

Mirrors Phase A's H3/M5
(`dal-cpp/dal/curve/calibration.cpp:385-389`). Ineligibility routes through
NOTICE + `nullptr` (solver dense-bumps). No `REQUIRE` or `THROW` on the
ineligibility path. The only `REQUIRE`s in `AnalyticJacobian` are the
structural invariants (positive annuity at `ycinstrument.cpp:357`, the
`x.size() == totalFreeParams` length check), which throw on programming
errors not on ineligibility.

## 8. Jacobian Subclass Reuse (spec OQ-2) — RESOLVE

### 8.1 The situation

`XCurveJacobian_` (`dal-cpp/dal/curve/calibration.cpp:43-81`) is structurally
identical to what the joint path needs: dense `Matrix_<>` storage, the six
`Underdetermined::Jacobian_` virtuals (`Rows`, `Columns`, `DivideRows`,
`MultiplyRight`, `MultiplyLeft`, `QForm`, `SecantUpdate`). It is declared in
`Dal::` (not anonymous — see the comment at `:37-42` explaining why: the
calibration flow constructs it and the solver's virtual interface reads its
contents). The joint path needs the same six virtuals against a different-
shape matrix.

### 8.2 Resolution: factor `XCurveJacobian_` into a shared header

**Factor `XCurveJacobian_` into a new shared header
`dal-cpp/dal/curve/curvejacobian.hpp`**, and have both `calibration.cpp` and
`jointcalibration.cpp` include it. In the same PR, refactor
`calibration.cpp`'s `#include` and remove its local definition.

Why factor (not port-verbatim into `jointcalibration.cpp`'s anonymous
namespace, which was the spec's option (a)):

- The subclass is non-trivial (~40 lines, 6 virtuals, the `QForm`/`SecantUpdate`
  bodies have real arithmetic). Porting it creates a second copy that must
  track the first — exactly the maintenance hazard Phase A's `PrecomputeT`/
  `Precompute` split risked (`aad-analytic-jacobian-phase-a-plan.md` §7.5).
- The subclass is already in `Dal::` (not anonymous) precisely so it can be
  shared; the comment at `dal-cpp/dal/curve/calibration.cpp:37-42` says so.
  Phase B is the second consumer the comment anticipated.
- The refactor of `calibration.cpp` is mechanical (delete the local
  definition, add `#include <dal/curve/curvejacobian.hpp>`). The diff is small
  and reviewable. The spec's concern ("(b) only if the single-curve path is
  also refactored to use it in the same PR — otherwise the diff sprawls") is
  satisfied by doing both in the same PR.

Why not define a joint-local variant (option (c)): same maintenance hazard as
(a), plus a third name for the same concept.

The header is minimal:

```cpp
// dal-cpp/dal/curve/curvejacobian.hpp  (illustrative)
#pragma once
#include <dal/math/matrix/matrixs.hpp>
#include <dal/math/optimization/underdetermined.hpp>
#include <dal/utilities/functionals.hpp>

namespace Dal {
    // Dense Jacobian subclass for curve calibration. Storage is dense regardless
    // of how the matrix is filled; assembly is sparse-by-row because AAD produces
    // exact structural zeros. Used by both single-curve (calibration.cpp) and
    // joint multi-curve (jointcalibration.cpp) AAD paths.
    struct XCurveJacobian_ : Underdetermined::Jacobian_ {
        Matrix_<> j_;
        explicit XCurveJacobian_(Matrix_<>&& j) : j_(std::move(j)) {}
        // ... six virtuals, verbatim from calibration.cpp:43-81 ...
    };
}  // namespace Dal
```

## 9. API Shape (spec OQ-4, OQ-5 resolved above) — RESOLVE

### 9.1 The options surface (spec OQ-4)

Resolution: **a new `JointMultiCurveCalibrationOptions_` struct**, sibling of
`JointMultiCurveCalibrationSpec_`, holding the `CurveJacobianMode_` flag.
NOT on the spec, NOT on `JointCurveDeclaration_` (mirrors Phase A's H2,
`.claude/designs/aad-analytic-jacobian-naming-and-flag.md:376-401`).

Why a new joint options struct (not reuse `CurveCalibrationOptions_`):

- The single-curve options struct is a sibling of `CurveCalibrationSpec_`
  (`dal-cpp/dal/curve/calibration.hpp:101-103`). The joint spec is a different
  struct (`JointMultiCurveCalibrationSpec_`); reusing the single-curve options
  would couple the joint spec to the single-curve spec's shape and confuse
  callers ("why does the joint calibration take the single-curve options?").
- A joint options struct leaves room for future joint-specific solver knobs
  (line-search flag, diagnostics verbosity) without touching the single-curve
  surface.
- Symmetry: the joint module already has a spec/diagnostics/result trio
  (`jointcalibration.hpp:39-94`); the options struct slots in as the fourth
  peer, mirroring the single-curve quartet.

Why NOT on `JointCurveDeclaration_`: a per-declaration `jacobianMode_` would
allow mixed modes (declaration 0 analytic, declaration 1 bumped), which is
incoherent — the joint AAD path is all-or-nothing (§7.1). The mode is a
property of the JOINT solve, not of any one declaration.

Why NOT on `JointMultiCurveCalibrationSpec_`: serialization semantics. The
spec round-trips (or will round-trip) through Machinist `String()` serialization;
the mode is a per-solve choice, not a property of the curve set (Phase A H2,
same reasoning). `jacobianMode_` is NOT serialized with the spec.

### 9.2 The default (spec FR4) — ANALYTIC, matching single-curve

**Default `ANALYTIC`.** A default-constructed `JointMultiCurveCalibrationOptions_`
engages the AAD path on eligible joint specs. This MATCHES the single-curve
default (`dal-cpp/dal/curve/calibration.hpp:102`), removing the asymmetry the
earlier Option-A draft introduced. The existing single-arg
`CalibrateJointMultiCurve(spec)` (`dal-cpp/dal/curve/jointcalibration.hpp:99`)
delegates to a new two-arg overload with default-constructed options (→
ANALYTIC), so existing callers exercise the AAD path by default after the
upgrade. On an ineligible spec, the NOTICE fires and the bumped result is
byte-for-byte identical to the pre-Phase-B path (spec AC8).

**Why ANALYTIC (not the earlier draft's BUMPED):** the earlier Option-A draft
defaulted to BUMPED on the grounds that Phase B ships new templated machinery
with no production track record. Under Option B the user has LOCKED the
decision to match single-curve, accepting that every existing joint caller
now exercises the new `Tape::DiscountPWLF_<T_>` + `Tape::JointCurveBlock_<T_>`
machinery on their first run after the upgrade. The mitigation is the
oracle test (spec AC1) and the four-backend build matrix (spec AC6): if the
new machinery is correct on all four backends, defaulting to ANALYTIC is
safe; if it is not, the oracle test fails loud before merge. The migration
note in the changelog states the default flip explicitly.

### 9.3 Composition with `solveMode_` (spec FR3 (d))

Phase A captures an at-solution forward Jacobian only under
`ANALYTIC && EXACT && eligible` (`dal-cpp/dal/curve/calibration.cpp:824`).
The joint path inherits the same constraint:

- `ANALYTIC && eligible` → the AAD Jacobian engages in `Gradient` regardless
  of `solveMode_` (EXACT or APPROXIMATE). The Jacobian is well-defined in
  both; APPROXIMATE just uses it differently.
- The at-solution forward-Jacobian capture (§9.4) is EXACT-only, matching
  Phase A. Under APPROXIMATE, no forward J is captured.

### 9.4 At-solution forward Jacobian on the joint result (spec OQ-6, "Risk work enablement")

Resolution: **DEFER the joint at-solution forward-Jacobian capture out of
Phase B.** The single-curve `CurveCalibrationDiagnostics_::jacobian_`
(`dal-cpp/dal/curve/calibration.hpp:129`) is a separable diagnostics feature;
the joint analogue is a separable follow-up.

Why defer:

- The core Phase B deliverable (G1: replace the `nullptr` in
  `JointResidualFunction_::Gradient`) does not depend on capturing the at-
  solution forward J. The solver's convergence-branch hook
  (`Underdetermined::Find`'s `fwd_jacobian_at_solution` out-param at
  `dal-cpp/dal/math/optimization/underdetermined.hpp:80`) is what captures
  it; that hook works for any `Function_::Gradient` override, joint or
  single-curve.
- The joint diagnostics struct (`JointCurveCalibrationDiagnostics_`,
  `dal-cpp/dal/curve/jointcalibration.hpp:72-82`) has no `jacobian_` field
  today. Adding one (per-declaration, shape `nInstruments_d x nFreeParams_d`)
  is a new field on a public struct — a surface expansion that deserves its
  own design pass (does it go on the per-curve diagnostics, or on the joint
  result as a block-diagonal matrix? how do downstream consumers read it?).
- The risk-work enablement the spec calls "the payoff" is real but staged:
  Phase B delivers the cheap Jacobian INSIDE the solver (every restart, one
  sweep per row instead of P+1 residual evals); the at-solution forward J
  for downstream risk is a Phase B+1 deliverable that consumes the same
  machinery.

### 9.5 Concrete `Gradient` signature

The override signature is unchanged from Phase A's shape
(`dal-cpp/dal/curve/calibration.cpp:368`):

```cpp
[[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override;
```

The body becomes a dispatch mirroring Phase A's at `:368-389`:
BUMPED short-circuit → nullptr; ANALYTIC → cached eligibility check →
`AnalyticJacobian(x, f)` or nullptr. The `JointResidualFunction_` gains a
`CurveJacobianMode_ jacobianMode_` member (from the options), an
`Eligibility_ cachedEligibility_` member, and the `EvaluateEligibilityOnce`
lazy evaluator (verbatim from Phase A at `:396-400`).

## 10. Backend Neutrality Plan (spec G2, FR2) — RESOLVE

### 10.1 The facade suffices — NO new primitives

The `Dal::AAD` facade (`dal-cpp/dal/math/aad/aad.hpp`) exposes everything
the joint path needs: `RegisterIndependent`, `ZeroAdjoints`, `Adjoint`,
`PropagateToStart`, `NewRecording`, `Clear`, `Value`, `Tape()`. All four
backend blocks (native `:17-54`, Adept `:55-80`, XAD `:81-110`, CoDiPack
`:111-134`) implement them. The joint AAD cycle (§6.1) uses exactly these
primitives — no tape-walking, no `nodes_` access, no backend-specific
identifier plumbing. The single-result loop is correct-by-construction on
all four backends (Phase A's B1/B2 fixes apply unchanged).

**No new facade primitive is required for Phase B.** This is the headline
backend result: the multi-curve templating adds types and routing, not
facade surface.

### 10.2 The four seams Phase A fixed carry over verbatim

- **Seam 1 (independent registration):** `RegisterIndependent` is called for
  every declaration's `2 * nKnots` free parameters (§6.2 step 1). The XAD
  block REQUIREs the tape is active (the B2 loud-not-silent guarantee at
  `aad.hpp:103`).
- **Seam 2 (ZeroAdjoints):** `ZeroAdjoints` is called between rows (§6.2
  step 6). The Adept block does real work via `ZeroGradientArray`
  (`aad.hpp:78`, the B1 fix).
- **Recording window:** `NewRecording` is opened AFTER `RegisterIndependent`
  and BEFORE the forward pass (§6.2 step 2). The XAD block's comment at
  `aad.hpp:96-100` states the contract; the order is the same as Phase A's.
- **`static_cast<double>(Number_)`:** BANNED. The joint path uses
  `Dal::AAD::Value(...)` everywhere a `Number_` must be extracted as `double`
  (e.g. the annuity positivity check, mirroring Phase A at
  `dal-cpp/dal/curve/ycinstrument.cpp:357`, and the adjoint harvest at §6.2
  step 6). The one `static_cast<double>` in the body is on
  `slot.marketRates[i]` (a `double`, not a `Number_`) — legal and intended.

### 10.3 Explicit-instantiation gates (the seam-3 cleanup, generalized)

Phase A's explicit-instantiation gates
(`dal-cpp/dal/curve/yclogdf.cpp:482-484`, `dal-cpp/dal/curve/ycinstrument.cpp:511-518`)
were removed in the Phase A ship (the facade made them redundant, per
`aad-analytic-jacobian-redesign.md` "Implementation realities" item 5 / seam 3).
The joint path's new templated machinery needs its own explicit
instantiations:

- `template class Tape::DiscountPWLF_<Dal::AAD::Number_>;` (the baseless
  variant — §5.2, default `B_ = DiscountCurve_<double>`).
- `template class Tape::DiscountPWLF_<Dal::AAD::Number_,
  DiscountCurve_<Dal::AAD::Number_>>;` (the base-layered variant — §5.4).
  This is a NEW instantiation that Phase A never needed.
- The projection-capable rate classes are anonymous-namespace in
  `jointcalibration.cpp`; they are NOT explicitly instantiated (they are
  used only in the `AnalyticJacobian` body, which is itself in the same TU).
- `BuildDeclarationCurveT<Dal::AAD::Number_>` is a free function template
  used only in `jointcalibration.cpp`; not explicitly instantiated.

No `#if` gates around any of these (matching shipped Phase A — the runtime
enum + facade replaced the gates, per Decision 6 of `redesign.md`).

### 10.4 Four-backend build + verification (spec AC6)

The build matrix runs all four CMake presets:

| Preset | Build dir | Backend | Joint AAD path engages? |
|--------|-----------|---------|-------------------------|
| `Release-linux` (default) | `build/` | native | Yes |
| `Release-linux` + `-DDAL_USE_XAD_AAD=ON` | `build-xad/` | XAD | Yes |
| `Release-linux` + `-DDAL_USE_CODIPACK_AAD=ON` | `build-codi/` | CoDiPack | Yes |
| `Release-linux` + `-DDAL_USE_ADEPT_AAD=ON` | `build-adept/` | Adept | Yes |

Test binaries run from `build/<sub>/` (per the spec's hard constraint and
the `bin/` stale-install trap in user memory). The joint AAD test suite
(the new `test_joint_analytic_jacobian.cpp`, §11) runs under each preset;
the `SKIP_IF_NO_ANALYTIC_JACOBIAN()` macro is NOT used (the joint AAD path
must RUN, not skip, on every backend — spec AC6).

## 11. Testing Strategy (spec AC1-AC7)

The test file is `dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`,
suite `JointAnalyticJacobianTest`, following
`.claude/rules/unit-test-style.md`. Tests mirror Phase A's
(`dal-cpp/tests/curve/test_analytic_jacobian.cpp`) plus the joint-specific
cross-curve coupling tests:

| Test | Spec AC | What it asserts |
|------|---------|-----------------|
| `TestMatchesCentralDifferences` | AC1 | AAD Jacobian element-wise `ASSERT_NEAR(aad, fd, 1e-6)` relative against a central FD bump (1e-6) of the joint `F`. The FD oracle is backend-independent; an all-zero AAD row against a non-zero FD row fails loud. Run under each of `build/`, `build-adept/`, `build-xad/`, `build-codi/`. |
| `TestPerRowNonTrivialInvariant` | AC2 | For every residual row i, at least one column j has `|jac(i,j)| > 1e-6`. Catches the B2 silent-zero class. |
| `TestCrossRowCleanliness` | AC3 | Two-row problem with OIS rows and IBOR rows on disjoint parameter sub-vectors; assert row i's Jacobian is identical whether row i-1 was swept first (run twice with row order swapped, assert element-wise equal to 1e-12). Catches the B1 Adept residue class. |
| `TestCrossCurveCouplingCaptured` | AC4 | An OIS knot perturbation produces non-zero sensitivity in at least one IBOR row (via discounting), AND an IBOR forward knot perturbation produces non-zero sensitivity in at least one IBOR row (via forecast). Both non-zero in AAD and match FD. |
| `TestBaseHandlePropagation` | AC5 | With a base-layered forward declaration, an OIS knot perturbation produces non-zero sensitivity in the forward declaration's own residual rows THROUGH the base handle (beyond the IBOR discounting channel). Non-zero in AAD and matches FD. This is the load-bearing Option-B test: the templated base handle in `Tape::DiscountPWLF_<T_, DiscountCurve_<T_>>` must propagate adjoints into the OIS discount curve on every backend. |
| `TestPwlForwardIntegrationOnTape` | AC1 (extended) | A single-declaration PWL_FWD discount curve (no forward, no base layering — the simplest case) has its AAD Jacobian match FD element-wise. Isolates the `Tape::DiscountPWLF_<T_>` forward-integration-on-`T_` correctness from the multi-curve routing. Run under each backend. |
| `TestEligibilityNoticesOnce` | AC7 | An ANALYTIC joint spec with each ineligibility clause violated (non-PWL_FWD declaration, out-of-scope instrument, projection violation on a discount declaration, missing base-collateral declaration, tradeDate != knot 0) emits the expected NOTICE exactly once across a full `CalibrateJointMultiCurve` run, then falls back to bumped and converges. Verified structurally (the NOTICE stack has no counter; the cache guarantees once-per-call). |
| `TestBumpedFallbackIsByteForByte` | AC8 | A joint options constructed with `jacobianMode_ = BUMPED` produces a result identical (calibrated node values within solver tolerance, `solverEvaluations_` in the same ballpark) to the current single-arg call on the same spec. |

All eight run under each of the four backends (AC6). The first six are the
correctness falsifiers; the last two are the contract tests.

## 12. Risk Work Enablement (spec §7 "the payoff") — RESOLVE

### 12.1 What Phase B delivers (inside the solver)

Phase B's AAD Jacobian replaces `(P+1) × cost(F)` dense-bumping with one
forward recording + `nRows` reverse sweeps per `Gradient` call. For the
example's joint system (2 declarations, 24 instruments, 9 knots/decl →
18 params/decl, 36 total), dense-bumping costs ~37 residual evals per
Jacobian; the AAD path costs 1 forward + 24 sweeps. The win scales with P.

This is the in-solver payoff and it is what Phase B ships.

### 12.2 What Phase B defers (downstream risk)

The at-solution forward Jacobian on the joint result (the analogue of
`CurveCalibrationDiagnostics_::jacobian_` at
`dal-cpp/dal/curve/calibration.hpp:129`) is deferred (§9.4, OQ-A). The
downstream risk consumer (the joint analogue of the
`yield_curve_jacobian` example at
`dal-cpp/examples/yield_curve_jacobian/`) is a Phase B+1 deliverable.

## 13. Open Questions for the User

- **OQ-A (risk work enablement).** Defer the joint at-solution forward-
  Jacobian capture (§9.4) to Phase B+1, or scope it into Phase B? Recommend
  defer; the in-solver payoff (§12.1) is the load-bearing deliverable.
- **OQ-B (double-path cleanup).** The existing anonymous-namespace
  `DiscountPWLF_` at `ycimp.cpp:56-83` is the storable/serializable double
  curve. Phase B's `Tape::DiscountPWLF_<double>` is byte-for-byte identical
  in arithmetic. Should the double path switch to the templated class (one
  curve implementation, storable via the templated `Write`), or keep both
  (the anonymous-namespace double class for storage, the templated class
  for the AAD path)? Recommend KEEP BOTH for Phase B (the storage path is
  unchanged, zero regression risk); factor in a follow-up.

(Note: the earlier draft's OQ-B re: `OISSwap_` coverage and OQ-C re: the
joint default are RESOLVED — `OISSwap_` rides `Swap_` (spec OQ-1), and the
default is LOCKED to ANALYTIC (§9.2). The earlier OQ-3 re: the
LOG_DISCOUNT anchor reconciliation is MOOT under Option B (§3).)

Plus the spec's own OQ-2 (Jacobian subclass reuse — this design §8 recommends
factoring into a shared header), OQ-3 (compile-time gate — this design says
NO gate, matching shipped Phase A), OQ-4 (options struct shape — this design
§9.1 recommends a new joint options struct), OQ-5 (templated rate design —
this design §4.1 recommends new projection-capable subclasses).

## 14. The Option-B Risk Surface (what the re-critique must stress)

The earlier Option-A critique raised blockers B1 (`OISSwap_` out of scope),
B2 (`BuildJointSmoothing` free-knot iteration), and B3 (anchor audit).
Under Option B:

- **B1 is DISSOLVED.** `OISSwap_ : public Swap_` and rides
  `Swap_::PrecomputeT<T_>()` (spec OQ-1 RESOLVED). No new templated rate is
  authored for OIS.
- **B2 is CLOSED.** PWL_FWD has 2 params/knot with NO anchor exclusion;
  `BuildJointSmoothing` already expands every knot by `paramsPerKnot == 2`.
  No free-knot rework (spec OQ-7).
- **B3 is CLOSED.** PWL_FWD has NO today-pinned anchor; the joint
  validator's `knotDates_.front() > today_` rule stands unchanged. No
  relaxation, no anchor audit (spec OQ-7).

The Option-B risk surface that replaces B1/B2/B3, and that the re-critique
MUST stress, is the NEW templated machinery:

1. **Forward-to-log-DF integration on `T_`.** The `T_`-typed `IntegralTo`
   inside `Tape::DiscountPWLF_<T_>` is a piecewise-linear-in-forward
   accumulation whose trapezoidal weights are `double` knot-abscissa
   differences and whose values are `T_` forward parameters. An off-by-one
   in the segment index, a wrong trapezoid weight, or a missed
   `RegisterIndependent` on one of `fLeftT_`/`fRightT_` silently yields a
   wrong Jacobian. The `TestPwlForwardIntegrationOnTape` test (§11) isolates
   this from the multi-curve routing. The reverse sweep must propagate
   adjoints through the `Dal::AAD::exp(-integral/365.0)` correctly on every
   backend — `Dal::AAD::exp` is exercised by Phase A but the
   forward-integration accumulation feeding it is new.
2. **Templated base handle across four backends.**
   `Tape::DiscountPWLF_<Number_, DiscountCurve_<Number_>>` is the first
   templated curve in the tree to carry a `T_`-typed base (Phase A's
   `DiscountLogDF_<T_>` uses a double base treated as constant). The reverse
   sweep must propagate adjoints through the `(*base_)(from, to)`
   multiplication into the OIS discount curve's free parameters on every
   backend. The `TestBaseHandlePropagation` test (§11, spec AC5) is the
   load-bearing falsifier: a base handle that silently detaches on Adept
   (the B1 residue class) would pass the cross-row cleanliness test but fail
   this one.
3. **2-params/knot column map (no anchor exclusion).** Phase A's column map
   is `solver col j = storage node j+1` (anchor excluded, `nKnots - 1`
   columns). Option B's column map is `solver col (paramOffset + 2k) =
   fLeftT[k]`, `solver col (paramOffset + 2k+1) = fRightT[k]` (NO anchor
   exclusion, `2 * nKnots` columns). A column-map off-by-one that works
   under LOG_DISCOUNT's `nKnots - 1` would silently misalign under PWL_FWD's
   `2 * nKnots`. The harvest step at §6.2 step 6 must be read carefully;
   the `TestMatchesCentralDifferences` test (§11, spec AC1) falsifies any
   misalignment.

These three are the load-bearing Option-B concerns. The re-critique should
attack them directly; everything else carries over from Phase A verbatim.

## 15. Acceptance Criteria Enabled

This design enables every spec AC:

- AC1 (AAD-vs-bumped agreement) — §6 templated residual + §11 `TestMatchesCentralDifferences` + `TestPwlForwardIntegrationOnTape`.
- AC2 (per-row non-trivial) — §11 `TestPerRowNonTrivialInvariant`.
- AC3 (cross-row cleanliness) — §11 `TestCrossRowCleanliness`.
- AC4 (cross-curve coupling) — §2 `JointCurveBlock_<T_>` + §4 projection-capable rates + §11 `TestCrossCurveCouplingCaptured`.
- AC5 (base-handle propagation) — §5 `Tape::DiscountPWLF_<T_, DiscountCurve_<T_>>` + §11 `TestBaseHandlePropagation`.
- AC6 (four-backend engagement) — §10 facade sufficiency, no new primitives, run from `build/<sub>/`.
- AC7 (eligibility NOTICEs once) — §7 cached verdict + §11 `TestEligibilityNoticesOnce`.
- AC8 (BUMPED fallback byte-for-byte) — §9.2 ANALYTIC default + §11 `TestBumpedFallbackIsByteForByte`.
- AC9 (no regression) — §11 full `bin/dal_cpp_tests` green; existing joint tests unchanged (the default flipped to ANALYTIC, but existing eligible specs now exercise the AAD path — tests are updated in implementation to assert AAD-matches-bumped).
- AC10 (code style) — §10.2 `Dal::AAD::Value` everywhere; anonymous-namespace helpers; no hand-edited Machinist enums; the `CurveJacobianMode_` enum is reused unchanged (no Machinist regen required for the flag — spec NG7); `Tape::DiscountPWLF_<T_>` follows the `Tape::DiscountLogDF_<T_>` header/source layout.

## 16. Hand-Off

- **Design doc path (absolute):**
  `/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib/.claude/designs/joint-aad-gradient.md`
- **API note path (absolute):**
  `/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib/.claude/api-notes/joint-aad-gradient.md`
- **Blocking dependencies:** NONE — Option B is LOCKED; `OISSwap_` coverage
  is RESOLVED (rides `Swap_`); B2/B3 are RE-DERIVED and CLOSED under PWL_FWD.
  The design is ready for critique and implementation.
- **Next agent:** `dal-critic` for an adversarial pass that STRESSES the
  Option-B risk surface (§14: templated PWL-forward integration on `T_`,
  templated base handle across four backends, 2-params/knot column map),
  the `Tape::DiscountPWLF_<T_>` class design (§5), the `XCurveJacobian_`
  shared-header refactor (§8), and the projection-capable rate design (§4).
  Then `dal-implementer` once the critic signs off.
- **Suggested branch:** `feature/joint-aad-gradient` off
  `feature/multi-curve-joint-calibration`. Per project memory: do NOT merge;
  the user merges.
